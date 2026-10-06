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

	void Enter(PlayerRef player);
	void Leave(uint64_t playerId);

	void HandleClientPacket(uint64_t sessionID, uint8_t* payload, uint16_t size);
	void HandleMove(uint64_t playerId, float x, float y, float z);

	// 포탈 진입 -> 잔여 큐 소진(Drain) & 저장 & Gateway 핸드셰이크 트리거
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