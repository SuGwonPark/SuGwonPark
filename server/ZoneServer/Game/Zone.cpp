#include "pch.h"
#include "Zone.h"
#include "Player.h"
#include "DB/GameDB.h"
#include "Protocol/InternalPackets.h"
#include "Network/GatewaySession.h"
#include "Manager/SendBufferManager.h"
#include "Game/ZoneData.h"


Zone::Zone(uint32_t zoneID, uint32_t channelID)
	: zoneID_(zoneID)
	, channelID_(channelID) {
}

void Zone::Enter(PlayerRef player) {
	players_[player->GetPlayerID()] = player;
	sessionToPlayer_[player->GetSessionID()] = player;
	// 주변 시야(AOI) 내 유저들에게 입장 알림 브로드캐스트
}

void Zone::Leave(uint64_t playerID) {
	auto it = players_.find(playerID);
	if (it == players_.end())return;

	sessionToPlayer_.erase(it->second->GetSessionID());
	players_.erase(it);
}

void Zone::HandleDisconnect(uint64_t sessionID) {
	auto self = shared_from_this();
	// 스케쥴 시작
	Post([self, sessionID]() {
		// 세션 ID 확인 없으면 종료
		auto it = self->sessionToPlayer_.find(sessionID);
		if (it == self->sessionToPlayer_.end()) return;

		// 세션 캐릭터 정보
		PlayerRef player = it->second;
		// 캐릭터 정보 저장
		GameDB::Instance().SaveCharacterProgressAsync(
			player->GetPlayerID(), player->GetExp(),
			player->GetX(), player->GetY(), player->GetZ(), player->GetHp(), player->GetMp(), self->GetZoneID());

		// 퇴장 처리
		self->Leave(player->GetPlayerID());
		});
}

void Zone::HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size) {
	if (size < sizeof(PacketHeader))
		return;

	std::vector<uint8_t> copied(payload, payload + size);

	auto self = shared_from_this();
	Post([self, sessionID, copied = std::move(copied)]() mutable {
		PacketHeader* header = reinterpret_cast<PacketHeader*>(copied.data());

		switch (header->id) {
			// 스폰 준비
		case PKT_C_READY_TO_SPAWN:
			self->HandleReadyToSpawn(sessionID, copied.data(), static_cast<uint16_t>(copied.size()));
			break;

			// 캐릭터 이동
		case PKT_C_MOVE: {
			auto it = self->sessionToPlayer_.find(sessionID);

			// 아직 Enter 안 한 세션이 이동 패킷 보냄 -> 무시
			if (it == self->sessionToPlayer_.end()) return;

			auto* move = reinterpret_cast<MovePacket*>(copied.data());

			self->HandleMove(it->second->GetPlayerID(), move->x, move->y, move->z);
			break;
		}
		default:
			break;
		}
		});
}

void Zone::HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size) {
	if (size < sizeof(REQ_ReadyToSpawnPacket)) return;
	uint64_t playerID = reinterpret_cast<REQ_ReadyToSpawnPacket*>(payload)->playerID;

	// 중복 스폰 방지
	if (sessionToPlayer_.count(sessionID)) return;

	auto self = shared_from_this();

	GameDB::Instance().PushTask([self, sessionID, playerID](mysqlx::Session& db) {
		try {
			auto row = db.sql("SELECT c.name, cs.PosX, cs.PosY, cs.PosZ, cs.hp, cs.mp, cs.exp "
				"FROM `character` c JOIN `character_state` cs "
				"ON c.playerID = cs.playerID "
				"WHERE c.playerID = ? AND c.deleteTime IS NULL")
				.bind(playerID).execute().fetchOne();
			if (!row) return;

			std::string name = row[0].get<std::string>();
			float x = row[1].get<float>(), y = row[2].get<float>(), z = row[3].get<float>();
			int hp = row[4].get<int>(); int mp = row[5].get<int>();
			uint64_t exp = row[6].get<uint64_t>();

			self->Post([=]() {
				if (self->sessionToPlayer_.count(sessionID)) return;   // 조회 중 중복 요청 방지

				auto player = std::make_shared<Player>(playerID, name, x, y, z, hp, mp, exp);
				player->SetSessionID(sessionID);
				self->Enter(player);
				});
		}
		catch (const mysqlx::Error& e) {
			std::cerr << "[Spawn] DB 로드 실패:" << e.what() << std::endl;
		}
		});

}

void Zone::HandleMove(uint64_t playerID, float x, float y, float z) {
	if (portalPending_.count(playerID)) return;    // 포탈 저장 중에는 위치를 고정

	auto it = players_.find(playerID);
	if (it == players_.end()) return;

	it->second->SetPosition(x, y, z);

	MovePacket pkt{};
	pkt.header.size = sizeof(MovePacket);
	pkt.header.id = PKT_C_MOVE;
	pkt.playerID = playerID;
	pkt.x = x;
	pkt.y = y;
	pkt.z = z;

	// 지금은 Zone 전체에 뿌린다.
	for (auto& [otherId, other] : players_) {
		if (otherId == playerID) continue;
		GatewaySession::GetInstance()->SendToClient(
			other->GetSessionID(), zoneID_, channelID_, &pkt, sizeof(pkt));
	}
}

void Zone::HandlePortal(uint64_t playerID, uint32_t nextZoneID) {
	auto it = players_.find(playerID);
	if (it == players_.end()) return;

	auto sp = FindSpawnPoint(nextZoneID);
	if (!sp) return;

	// 이미 포탈 처리 중이면 무시
	if (portalPending_.insert(playerID).second) return;

	PlayerRef player = it->second;
	const uint64_t sessionID = player->GetSessionID();

	auto self = shared_from_this();

	GameDB::Instance().SaveCharacterProgressAsync(
		playerID, player->GetExp(), sp->x, sp->y, sp->z,
		player->GetHp(), player->GetMp(), nextZoneID,
		// 콜백 DB 스레드에서 실행
		[self, player, sessionID, playerID, nextZoneID](bool ok) {
			self->Post([=]() {
				// 포탈 로딩 종료
				self->portalPending_.erase(playerID);
				if (!ok) return;

				// 1. Zone 월드에서 서버 제거 (추가 피격/상호작용 방지)
				self->Leave(playerID);

				RES_ZoneLeaveCompletedPacket ackPkt{};
				ackPkt.header.size = sizeof(ackPkt);
				ackPkt.header.id = PKT_S_ZONE_LEAVE_COMPLETED;
				ackPkt.playerID = playerID;
				ackPkt.nextZoneID = nextZoneID;

				GatewaySession::GetInstance()->SendToClient(
					sessionID, self->zoneID_, self->channelID_, &ackPkt, sizeof(ackPkt));
				});
		});

}
