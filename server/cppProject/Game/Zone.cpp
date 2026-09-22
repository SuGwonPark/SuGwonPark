#include "pch.h"
#include "Zone.h"
#include "Player.h"
#include "DB/GameDB.h"
#include "Network/Protocol.h"
// TODO: GatewaySession 클래스가 아직 구현되지 않음 (ZoneServer <-> Gateway 통신 세션)
// #include "GatewaySession.h"

static WorkStealingThreadPool& GetZoneWorkerPool() {
	static WorkStealingThreadPool pool;   // 기본값: std::thread::hardware_concurrency()개 워커 생성
	return pool;
}

Zone::Zone(uint32_t zoneId)
	: zoneId_(zoneId)
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

void Zone::Leave(uint64_t playerId) {
	players_.erase(playerId);
	// 주변 시야 내 유저들에게 퇴장 알림 브로드캐스트
}

void Zone::HandleMove(uint64_t playerId, float x, float y, float z) {
	auto it = players_.find(playerId);
	if (it == players_.end()) return;

	it->second->SetPosition(x, y, z);

	MovePacket pkt{};
	pkt.header.size = sizeof(MovePacket);
	pkt.header.id = PKT_C_MOVE;
	pkt.playerId = static_cast<int32_t>(playerId);
	pkt.x = x;
	pkt.y = y;
	pkt.z = z;

	// 지금은 Zone 전체한테 뿌리는 가장 단순한 버전.
	// 나중에 players_를 순회할 때 거리 체크(x_,y_,z_ 비교)만 추가하면 진짜 AOI가 됩니다.
	for (auto& [otherId, other] : players_) {
		if (otherId == playerId) continue;
		other->Send(&pkt, sizeof(pkt));
	}
}

void Zone::HandlePortal(uint64_t playerId, uint32_t nextZoneId) {
	auto it = players_.find(playerId);
	if (it == players_.end()) return;

	PlayerRef player = it->second;

	// 1. Zone 월드에서 서버 제거 (추가 피격/상호작용 방지)
	Leave(playerId);

	// TODO: 아래 블록은 세 가지가 아직 구현되지 않아 주석 처리함
	//   - GameDB::SavePlayerData(...) 함수 자체가 GameDB 클래스에 없음
	//   - GatewaySession 클래스가 아직 없음
	//   - PKT_S_ZONE_LEAVE_COMPLETED 구조체가 Protocol.h에 주석으로만 남아있음 (되살려야 함)
	// 2. Redis/DB에 최신 상태(HP, 위치, 인벤토리) 비동기 저장
	// GameDB::Instance().SavePlayerData(player, [playerId, nextZoneId]() {
	// 	// 3. DB 저장이 완료되면 Gateway에 완료 통보(ACK) 전송
	// 	// -> Gateway가 이를 받고 대기 상태였던 클라이언트를 nextZoneId로 전환
	// 	PKT_S_ZONE_LEAVE_COMPLETED ackPkt;
	// 	ackPkt.header.size = sizeof(ackPkt);
	// 	ackPkt.header.id = PKT_S_ZONE_LEAVE_COMPLETED;
	// 	ackPkt.playerId = playerId;
	// 	ackPkt.nextZoneId = nextZoneId;
	//
	// 	GatewaySession::GetInstance()->Send(ackPkt);
	// 	});
}
