#pragma once
#include "Network/ClientProxySession.h"   // ClientProxySessionRef, Send를 쓰기 위해

class Player {
private:
	int id_;
	std::string name_;
	float x_;
	float y_;
	float z_;
	int hp_;
	uint64_t exp_;
	uint64_t playerId_;

	uint32_t sessionID_ = 0;

public:
	Player(int id, std::string name);

	void SetPosition(float x, float y, float z);   // ★ 추가
	void TakeDamage(int damage);

	uint64_t GetExp() const { return exp_; }
	void AddExp(uint64_t exp) { exp_ += exp; }

	uint32_t GetPlayerID() const { return playerId_; }
	void SetPlayerID(uint32_t playerId) { playerId_ = playerId; }

	uint64_t GetSessionID() { return sessionID_; }
	void SetSessionID(uint64_t sessionID) { sessionID_ = sessionID; }

	// ★ 추가: 세션 연결 + 패킷 전송
	void SetSession(uint32_t sessionID) { sessionID_ = sessionID; }
	void Send(const void* data, uint32_t length);
};