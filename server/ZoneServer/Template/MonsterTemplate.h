#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// Data/Monster.csv : ID,Name,MaxHp,Attack,MoveSpeed,AggroRange,AttackRange,RewardID,ResponSec
struct MonsterData {
	uint32_t    monsterID;
	std::string name;
	int32_t     maxHp;			// 최대 HP
	int32_t     attack;			// 공격력
	float       moveSpeed;		// 속도
	float       aggroRange;		// 어그로 범위
	float       attackRange;	// 공격 범위
	uint64_t    rewardID;		// 보상 정보
	int32_t     respawnSec;		// 리스폰 시간
};


class MonsterTemplate {
public:
	static MonsterTemplate& Instance() { static MonsterTemplate inst; return inst; }

	bool Load();
	const MonsterData* GetData(uint32_t monsterID) const;   // 없으면 nullptr
	const std::unordered_map<uint32_t, MonsterData>& All() const { return data_; }

private:
	std::unordered_map<uint32_t, MonsterData> data_;
};
