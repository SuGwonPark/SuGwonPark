#pragma once

class Player {
private:
	uint64_t userUID = 0; // 임시
	uint64_t playerID_;
	std::string name_;
	float x_;
	float y_;
	float z_;
	int hp_;
	int mp_;
	uint64_t exp_;

	uint64_t sessionID_ = 0;

public:
	Player(uint64_t playerID, std::string name, float x, float y, float z, int hp, int mp, uint64_t exp);

	void SetPosition(float x, float y, float z);   // ★ 추가

	uint64_t GetExp() const { return exp_; }
	void AddExp(uint64_t exp) { exp_ += exp; }

	uint64_t GetPlayerID() const { return playerID_; }
	void SetPlayerID(uint64_t playerId) { playerID_ = playerId; }

	uint64_t GetSessionID() const { return sessionID_; }
	void SetSessionID(uint64_t sessionID) { sessionID_ = sessionID; }

	float GetX() const { return x_; }
	float GetY() const { return y_; }
	float GetZ() const { return z_; }

	int GetHp() const { return hp_; }
	int GetMp() const { return mp_; }

	const std::string& GetName() const { return name_; }
};