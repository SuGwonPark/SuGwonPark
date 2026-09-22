#pragma once
#include "Network/ClientProxySession.h"   // ClientProxySessionRef, Send를 쓰기 위해

class Player {
private:
	int id_;
	std::string name_;
	float x_;
	float y_;
	float z_;              // ★ 추가: 3D 좌표
	int hp_;
	uint64_t exp_;
	uint64_t playerId_;

	ClientProxySessionRef session_;   // ★ 추가: 이 플레이어가 붙어있는 클라이언트 세션

public:
	Player(int id, std::string name);

	void Move(float x, float y);
	void SetPosition(float x, float y, float z);   // ★ 추가
	void TakeDamage(int damage);
	bool IsAlive() const;
	void PrintStatus() const;
	std::string GetPlayerName() const;

	uint64_t GetExp() const { return exp_; }
	void AddExp(uint64_t exp) { exp_ += exp; }
	int GetLevel() const;

	uint64_t GetPlayerID() const { return playerId_; }
	void SetPlayerID(uint64_t playerId) { playerId_ = playerId; }

	// ★ 추가: 세션 연결 + 패킷 전송
	void SetSession(ClientProxySessionRef session) { session_ = session; }
	void Send(const void* data, uint32_t length);
};