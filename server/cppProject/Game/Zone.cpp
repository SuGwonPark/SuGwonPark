#include "pch.h"
#include "Zone.h"
#include "Player.h"
#include "DB/GameDB.h"
#include "Network/Protocol.h"
#include "GatewaySession.h" // ZoneServer가 Gateway와 통신하는 세션

Zone::Zone(uint32_t zoneId)
	: zoneId_(zoneId)
	, jobQueue_(std::make_shared<JobQueue>()) {
}

void Zone::Enter(PlayerRef player) {
	players_[player->GetPlayerId()] = player;
	// 주변 시야(AOI) 내 유저들에게 스폰 알림 브로드캐스트
}

void Zone::Leave(uint64_t playerId) {
	players_.erase(playerId);
	// 주변 시야 내 유저들에게 디스폰 알림 브로드캐스트
}

void Zone::HandleMove(uint64_t playerId, float x, float y, float z) {
	auto it = players_.find(playerId);
	if (it == players_.end()) return;

	it->second->SetPosition(x, y, z);
	// 주변 플레이어에게 이동 패킷 동기화
}

void Zone::HandlePortal(uint64_t playerId, uint32_t nextZoneId) {
	auto it = players_.find(playerId);
	if (it == players_.end()) return;

	PlayerRef player = it->second;

	// 1. Zone 월드에서 먼저 제외 (추가 피격/상호작용 차단)
	Leave(playerId);

	// 2. Redis/DB에 최신 상태(HP, 위치, 인벤토리) 비동기 저장
	GameDB::GetInstance()->SavePlayerData(player, [playerId, nextZoneId]() {
		// 3. DB 저장이 끝나면 Gateway에 완료 통보(ACK) 전송
		// -> Gateway가 이를 받고 즉시 라우팅 대상을 nextZoneId로 전환
		PKT_S_ZONE_LEAVE_COMPLETED ackPkt;
		ackPkt.header.size = sizeof(ackPkt);
		ackPkt.header.id = PKT_S_ZONE_LEAVE_COMPLETED_ID;
		ackPkt.playerId = playerId;
		ackPkt.nextZoneId = nextZoneId;

		GatewaySession::GetInstance()->Send(ackPkt);
		});
}
