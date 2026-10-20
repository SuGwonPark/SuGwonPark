#pragma once
#include "pch.h"
#include "ServerCore/JobSystem/JobQueue.h"
#include "ServerCore/JobSystem/WorkStealingThreadPool.h" 
#include "ServerCore/JobSystem/JobSerializer.h"

class Player;
using PlayerRef = std::shared_ptr<Player>;

class Zone : public JobSerializer<Zone> {
public:
	explicit Zone(uint32_t zoneID, uint32_t channelID);
	~Zone() = default;

	uint32_t GetZoneID() const { return zoneID_; }
	uint32_t GetChannelID() const { return channelID_; }

	// 존 입장
	void Enter(PlayerRef player);
	// 존 퇴장
	void Leave(uint64_t playerId);

	// 종료
	void HandleDisconnect(uint64_t sessionID);

	void HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size);
	// 캐릭터 무브
	void HandleMove(uint64_t playerId, float x, float y, float z);

	// 존 이동
	void HandlePortal(uint64_t playerId, uint32_t portalID);

	// 포탈 이동
	void HandlePortalRequest(const PlayerRef& player, uint32_t portalID);

	// 플레이어에 전달
	void SendTo(const PlayerRef& player, const void* data, uint16_t size);
	// 모든 플레이어에 전달
	void BroadCast(const void* data, uint16_t size, uint64_t exceptPlayerID = 0);

	// 모든 플레이어 저장 요청 후 Zone 비우기. 끝나면 onDone 호출 (Zone 스레드에서)
	void SaveAllAndClear(std::function<void()> onDone = nullptr);
	void HandleGatewayLost() { SaveAllAndClear(); }      // 기존 함수는 이걸 부르도록
private:
	void HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size);

private:
	uint32_t zoneID_;
	uint32_t channelID_;
	std::unordered_map<uint64_t, PlayerRef> players_;
	std::unordered_map<uint64_t, PlayerRef> sessionToPlayer_; // sessionID -> Player (신규)
	std::unordered_set<uint64_t> portalPending_;   // 포탈 저장 중인 playerID
	std::unordered_set<uint64_t> spawnPending_; // 스폰 조회 중인 SessionID
};

using ZoneRef = std::shared_ptr<Zone>;