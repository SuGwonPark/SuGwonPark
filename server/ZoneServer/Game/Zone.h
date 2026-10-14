#pragma once
#include "pch.h"
#include "JobSystem/JobQueue.h"
#include "JobSystem/WorkStealingThreadPool.h" 

class Player;
using PlayerRef = std::shared_ptr<Player>;

class Zone : public std::enable_shared_from_this<Zone> {
public:
	explicit Zone(uint32_t zoneID, uint32_t channelID);
	~Zone() = default;

	uint32_t GetZoneID() const { return zoneID_; }
	uint32_t GetChannelID() const { return channelID_; }

	JobQueueRef GetJobQueue() { return jobQueue_; }

	void schedule(Job job);

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
	void HandlePortal(uint64_t playerId, uint32_t nextZoneId);

private:
	void HandleReadyToSpawn(uint64_t sessionID, uint8_t* payload, uint16_t size);

private:
	uint32_t zoneID_;
	uint32_t channelID_;
	JobQueueRef jobQueue_;
	std::unordered_map<uint64_t, PlayerRef> players_;
	std::unordered_map<uint64_t, PlayerRef> sessionToPlayer_; // sessionID -> Player (신규)
};

using ZoneRef = std::shared_ptr<Zone>;