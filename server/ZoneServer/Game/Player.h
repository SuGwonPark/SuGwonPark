#pragma once
#include "ServerCore/Protocol/VarEnum.h"
class Player {
private:
	uint64_t userUID = 0; // 임시
	uint64_t playerID_;
	std::string name_;
	float x_;
	float y_;
	float z_;
	int maxHp_ = 100; // 임시
	int hp_;
	int maxMp_ = 50; // 임시
	int mp_;
	uint64_t exp_;

	uint64_t sessionID_ = 0;

	// 마지막 이동 시간
	std::chrono::steady_clock::time_point lastMoveTime_ = std::chrono::steady_clock::now();
	float moveBudget_ = 0.f;

	// 마지막 공격 시간
	std::chrono::steady_clock::time_point lastAttackTime_{};

	// 스킬별 쿨타임
	std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> skillReadyAt_;

private:
	// 스킬 사용 처리
	void SpendSkillCost(SkillCostType type, int32_t cost);

public:
	Player(uint64_t playerID, std::string name, float x, float y, float z, int hp, int mp, uint64_t exp);

	void SetPosition(float x, float y, float z);  
	bool TryMove(float x, float y, float z, float maxSpeed);

	// 스킬 사용
	bool TrySpendSkill(SkillCostType type, int32_t cost);
	// 스킬 사용 가능 여부 체크
	bool CheckSKillCost(SkillCostType type, int32_t cost) const;

	// 자동 회복
	bool Regen(int32_t hpAmount, int32_t mpAmount);

	uint64_t GetExp() const { return exp_; }
	void AddExp(uint64_t exp) { exp_ += exp; }

	uint64_t GetPlayerID() const { return playerID_; }
	void SetPlayerID(uint64_t playerId) { playerID_ = playerId; }

	uint64_t GetSessionID() const { return sessionID_; }
	void SetSessionID(uint64_t sessionID) { sessionID_ = sessionID; }

	float GetX() const { return x_; }
	float GetY() const { return y_; }
	float GetZ() const { return z_; }

	int GetMaxHp() const { return maxHp_; }
	int GetMaxMp() const { return maxMp_; }
	int GetHp() const { return hp_; }
	int GetMp() const { return mp_; }

	const std::string& GetName() const { return name_; }

	bool IsDead() const { return hp_ <= 0; }

	int32_t TakeDamage(int32_t damage) {
		if (IsDead() || damage <= 0) return 0;
		const int32_t applied = damage < hp_ ? damage : hp_;
		hp_ -= applied;
		return applied;
	}

	void Revive(float x, float y, float z, int hp) {
		hp_ = hp;
		SetPosition(x, y, z);
	}

	// 공격 가능 여부 체크
	bool TryUseAttack(std::chrono::steady_clock::time_point now, std::chrono::milliseconds cooldown) {
		if (now - lastAttackTime_ < cooldown) return false;
		lastAttackTime_ = now;
		return true;
	}

	// 스킬 사용 가능 여부
	bool IsSkillReady(uint32_t skillID, std::chrono::steady_clock::time_point now) {
		auto it = skillReadyAt_.find(skillID);
		return it == skillReadyAt_.end() || now >= it->second;
	}

	// 스킬 쿨타임 적용
	void StartSkillCooldown(uint32_t skillID, std::chrono::steady_clock::time_point now, std::chrono::milliseconds cooldown) {
		skillReadyAt_[skillID] = now + cooldown;
	}


};