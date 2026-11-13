#include "pch.h"
#include "CombatStat.h"
#include "Game/ZoneConstants.h"
#include "ServerCore/Utils/util.h"

namespace
{
constexpr size_t STAT_COUNT = static_cast<size_t>(StatType::Max);
constexpr int32_t RATE_MAX = static_cast<int32_t>(BuffMaxPercent); // 10000 = 100%
constexpr int32_t MIN_ATTACK_SPEED = -RATE_MAX / 2;                // 공속 디버프 하한 (-50% → 쿨타임 2배)
constexpr double DEFENSE_CONSTANT = 100.0;                         // 방어력 100 = 받는 피해 50%

int32_t ToInt(double v)
{
    return static_cast<int32_t>(std::lround(v));
}

int32_t ClampI32(int64_t v)
{
    return static_cast<int32_t>(std::clamp<int64_t>(v, INT32_MIN, INT32_MAX));
}
} // namespace

CombatStat Combat::CalcStat(const CombatStat& base, const std::vector<StatModifier>& mods)
{
    // 스탯별 Flat / Percent 합 (디버프는 음수로 들어옴)
    int64_t flat[STAT_COUNT] = {};
    int64_t pct[STAT_COUNT] = {};
    for (const StatModifier& m : mods)
    {
        const size_t i = static_cast<size_t>(m.statType);
        if (i == 0 || i >= STAT_COUNT)
            continue;
        (m.valueType == ValueType::Percent ? pct : flat)[i] += m.value;
    }

    // 일반 스탯: (기본 + Flat) × (1 + Percent). 배율은 0 아래로 내려가지 않음
    auto normal = [&](StatType t, double baseValue) {
        const size_t i = static_cast<size_t>(t);
        const double mul = std::max(0.0, 1.0 + static_cast<double>(pct[i]) / BuffMaxPercent);
        return (baseValue + static_cast<double>(flat[i])) * mul;
    };

    // 확률/배율 스탯: 값 자체가 만분율이라 Flat/Percent 구분 없이 더함
    auto rate = [&](StatType t, int32_t baseValue) {
        const size_t i = static_cast<size_t>(t);
        return ClampI32(baseValue + flat[i] + pct[i]);
    };

    CombatStat r = base; // threatRate처럼 StatType이 없는 값은 그대로 유지
    r.maxHp = std::max(1, ToInt(normal(StatType::Hp, base.maxHp)));
    r.maxMp = std::max(0, ToInt(normal(StatType::Mp, base.maxMp)));
    r.damage = std::max(0, ToInt(normal(StatType::Damage, base.damage)));
    r.defense = std::max(0, ToInt(normal(StatType::Defense, base.defense)));
    r.hpRegen = std::max(0, ToInt(normal(StatType::HpRegen, base.hpRegen)));
    r.mpRegen = std::max(0, ToInt(normal(StatType::MpRegen, base.mpRegen)));
    r.moveSpeed = std::max(ZoneConst::MIN_MOVE_SPEED, static_cast<float>(normal(StatType::MoveSpeed, base.moveSpeed)));

    r.critRate = std::clamp(rate(StatType::CritRate, base.critRate), 0, RATE_MAX);
    r.critDamage = std::max(RATE_MAX, rate(StatType::CritDamage, base.critDamage));
    r.attackSpeed = std::max(MIN_ATTACK_SPEED, rate(StatType::AttackSpeed, base.attackSpeed));
    return r;
}

int32_t Combat::ApplyCritical(const CombatStat& attacker, int32_t damage)
{
    if (damage <= 0 || !Random::ChanceRate(attacker.critRate, RATE_MAX))
        return damage;
    return ToInt(static_cast<double>(damage) * attacker.critDamage / BuffMaxPercent);
}

int32_t Combat::ReduceByDefense(int32_t damage, const CombatStat& defender)
{
    if (damage <= 0)
        return 0;
    // 받는 비율 = 100 / (100 + 방어력), 최소 1
    return std::max(1, ToInt(damage * DEFENSE_CONSTANT / (DEFENSE_CONSTANT + defender.defense)));
}

int32_t Combat::AttackCooldownMs(int32_t baseMs, const CombatStat& attacker)
{
    // 공속 +100%(10000) → 쿨타임 절반, -50%(-5000) → 2배
    const int32_t speed = std::max(MIN_ATTACK_SPEED, attacker.attackSpeed);
    return std::max(1, ToInt(static_cast<double>(baseMs) * RATE_MAX / (RATE_MAX + speed)));
}