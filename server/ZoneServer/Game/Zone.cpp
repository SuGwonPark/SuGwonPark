#include "pch.h"
#include "Zone.h"
#include "Player.h"
#include "DB/GameDB.h"
#include "Network/Protocol.h"
#include "Network/GatewaySession.h"
#include "Manager/SendBufferManager.h"

static WorkStealingThreadPool& GetZoneWorkerPool() {
	static WorkStealingThreadPool pool;   // 기본값: std::thread::hardware_concurrency()개 워커 생성
	return pool;
}

Zone::Zone(uint32_t zoneID)
	: zoneID_(zoneID)
	, jobQueue_(std::make_shared<JobQueue>()) {
}

void Zone::schedule(Job job)
{
	jobQueue_->Push(std::move(job));

	// 이 Zone의 큐를 워커 하나가 드레인하도록 예약.
	// Execute()는 이미 누가 돌고 있으면 바로 리턴하니 중복 실행 걱정은 없음.
	auto self = shared_from_this();
	GetZoneWorkerPool().Post([self]() {
		self->jobQueue_->Execute();
		});
}

void Zone::Enter(PlayerRef player) {
	players_[player->GetPlayerID()] = player;
	// 주변 시야(AOI) 내 유저들에게 입장 알림 브로드캐스트
}

void Zone::Leave(uint64_t playerID) {
	players_.erase(playerID);
	// 주변 시야 내 유저들에게 퇴장 알림 브로드캐스트
}

void Zone::HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size) {
	if (size < sizeof(PacketHeader))
		return;

	std::vector<uint8_t> copied(payload, payload + size);

	auto self = shared_from_this();
	schedule([self, sessionID, copied = std::move(copied)]() mutable {
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
	auto* req = reinterpret_cast<REQ_ReadyToSpawnPacket*>(payload);

	// DB에서 마지막 캐릭터 정보 로드하여 저장
	PlayerRef player = std::make_shared<Player>(req->playerID, "playerName", 0, 0, 0, 100, 0);
	player->SetSessionID(sessionID);

	Enter(player);
	sessionToPlayer_[sessionID] = player;
}

void Zone::HandleMove(uint64_t playerID, float x, float y, float z) {
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

	// 지금은 Zone 전체한테 뿌리는 가장 단순한 버전.
	// 나중에 players_를 순회할 때 거리 체크(x_,y_,z_ 비교)만 추가하면 진짜 AOI가 됩니다.
	for (auto& [otherId, other] : players_) {
		if (otherId == playerID) continue;
		other->Send(&pkt, sizeof(pkt));
	}
}

void Zone::HandlePortal(uint64_t playerID, uint32_t nextZoneID) {
	auto it = players_.find(playerID);
	if (it == players_.end()) return;

	PlayerRef player = it->second;

	GameDB::Instance().SaveCharacterProgressAsync(
		playerID, player->GetExp(), player->GetX(), player->GetY(), player->GetZ(), player->GetHp()
	);

	// 1. Zone 월드에서 서버 제거 (추가 피격/상호작용 방지)
	Leave(playerID);

	RES_ZoneLeaveCompletedPacket ackPkt;
	ackPkt.header.size = sizeof(ackPkt);
	ackPkt.header.id = PKT_S_ZONE_LEAVE_COMPLETED;
	ackPkt.playerID = playerID;
	ackPkt.nextZoneID = nextZoneID;

	GatewaySession::GetInstance()->Send(SendBufferManager::Make(&ackPkt, sizeof(ackPkt)));



}
