#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "Game/CombatStat.h"

struct MonsterAI
{
    float aggroRange = 0.f;       // 어그로 범위
    float attackRange = 0.f;      // 공격 사거리
    float leashRange = 0.f;       // 일정 거리이상 멀어지면 복귀
    int32_t attackCooldownMs = 0; // 기본 공격 쿨타임
};

struct MonsterData
{
    uint32_t monsterID;
    std::string name;
    CombatStat stat; // 전투 기본값 (attack → stat.damage, maxHp → stat.maxHp ...)
    MonsterAI ai;    // 행동 수치
    uint32_t rewardGroupID;
    int32_t respawnSec;
};

class MonsterTemplate
{
  public:
    static MonsterTemplate& Instance()
    {
        static MonsterTemplate inst;
        return inst;
    }

    bool Load();
    const MonsterData* GetData(uint32_t monsterID) const; // 없으면 nullptr
    const std::unordered_map<uint32_t, MonsterData>& All() const
    {
        return data_;
    }

  private:
    std::unordered_map<uint32_t, MonsterData> data_;
};
