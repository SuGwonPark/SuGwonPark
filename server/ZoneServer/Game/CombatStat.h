#pragma once
#include <cstdint>
#include "ServerCore/Protocol/ProtocolTypes.h"

// 전투 스탯 (서버 전용). Player/Monster 공용으로 사용
struct CombatStat
{
    int32_t maxHp = 1;
    int32_t maxMp = 0;
    int32_t hpRegen = 0;
    int32_t mpRegen = 0;

    // 공격
    int32_t damage = 0;
    int32_t critRate = 0;
    int32_t critDamage = 15000; // 150%
    int32_t attackSpeed = 0;

    // 방어
    int32_t defense = 0;

    // 이동
    float moveSpeed = 0.f;

    // 어그로 게이지
    int32_t threatRate = 10000;
};

// 장비 옵션 / 버프 디버프 공용
// 디버프는 value를 음수로 세팅
struct StatModifier
{
    StatType statType;
    ValueType valueType;
    int32_t value;
};

inline StatInfo ToStatInfo(const CombatStat& cs)
{
    StatInfo info{};
    info.maxHp = cs.maxHp;
    info.maxMp = cs.maxMp;
    info.damage = cs.damage;
    info.defense = cs.defense;
    info.critRate = cs.critRate;
    info.critDamage = cs.critDamage;
    info.attackSpeed = cs.attackSpeed;
    info.moveSpeed = cs.moveSpeed;
    info.hpRegen = cs.hpRegen;
    info.mpRegen = cs.mpRegen;
    return info;
}
