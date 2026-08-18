#pragma once
#include "pch.h"
#include "JobSystem/JobQueue.h"

class Player;
using PlayerRef = std::shared_ptr<Player>;

class Zone : public std::enable_shared_from_this<Zone> {
public:
	explicit Zone(uint32_t zoneId);
	~Zone() = default;

	uint32_t GetZoneId() const { return zoneId_; }
	JobQueueRef GetJobQueue() { return jobQueue_; }

	// JobQueue 안에서 실행되는 게임 비즈니스 로직
	void Enter(PlayerRef player);
	void Leave(uint64_t playerId);
	void HandleMove(uint64_t playerId, float x, float y, float z);

	// 포탈 진입 -> 잔여 큐 소진(Drain) & 저장 & Gateway 핸드셰이크 트리거
	void HandlePortal(uint64_t playerId, uint32_t nextZoneId);

private:
	uint32_t zoneId_;
	JobQueueRef jobQueue_;
	std::unordered_map<uint64_t, PlayerRef> players_;
};

using ZoneRef = std::shared_ptr<Zone>;