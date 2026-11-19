#include "pch.h"
#include "StatusEffects.h"
#include "World/ZoneConstants.h"

namespace
{
// 버프 / 디버프 체크 (+ 버프, - 디버프)
bool SameDirection(int32_t a, int32_t b)
{
    return (a > 0) == (b > 0);
}
} // namespace

EffectResult StatusEffects::AddStat(StatType stat, ValueType valueType, int32_t value, Clock::time_point endAt)
{
    if (value == 0 || stat == StatType::None)
        return EffectResult::Ignored;

    auto it = std::find_if(stats_.begin(), stats_.end(), [&](const StatEffect& e) {
        return e.mod.statType == stat && e.mod.valueType == valueType && SameDirection(e.mod.value, value);
    });

    // 버프,디버프 신규 추가
    if (it == stats_.end())
    {
        stats_.push_back({{stat, valueType, value}, endAt});
        return EffectResult::Applied;
    }

    const int32_t oldPower = std::abs(it->mod.value);
    const int32_t newPower = std::abs(value);

    // 신규 추가
    if (newPower > oldPower)
    {
        *it = {{stat, valueType, value}, endAt};
        return EffectResult::Applied;
    }

    // 시간만 갱신
    if (newPower == oldPower)
    {
        it->endAt = std::max(it->endAt, endAt);
        return EffectResult::Refreshed;
    }

    // 갱신 안함
    return EffectResult::Ignored;
}

EffectResult StatusEffects::AddDot(
    uint64_t casterID, DotType dotType, int32_t damagePerTick, Clock::time_point now, Clock::time_point endAt)
{
    if (damagePerTick <= 0)
        return EffectResult::Ignored;

    auto it = std::find_if(
        dots_.begin(), dots_.end(), [&](const Dot& d) { return d.casterID == casterID && d.dotType == dotType; });

    // 도트 추가
    if (it == dots_.end())
    {
        dots_.push_back(
            {casterID, dotType, damagePerTick, now + std::chrono::milliseconds(ZoneConst::DOT_TICK_MS), endAt});
        return EffectResult::Applied;
    }

    // 도트 덮어쓰기
    if (damagePerTick > it->damagePerTick)
    {
        it->damagePerTick = damagePerTick;
        it->endAt = endAt;
        return EffectResult::Applied;
    }

    // 시간 갱신
    if (damagePerTick == it->damagePerTick)
    {
        it->endAt = std::max(it->endAt, endAt);
        return EffectResult::Refreshed;
    }
    return EffectResult::Ignored;
}

// 버프/ 디버프/ 도트 업데이트
bool StatusEffects::Update(Clock::time_point now, std::vector<StatModifier>& expired, std::vector<DotHit>& dotHits)
{
    auto mid = std::stable_partition(stats_.begin(), stats_.end(), [&](const StatEffect& e) { return now < e.endAt; });
    const bool statChanged = mid != stats_.end();
    for (auto it = mid; it != stats_.end(); ++it)
        expired.push_back(it->mod);

    // 종료된 버프,디버프 삭제
    stats_.erase(mid, stats_.end());

    const auto tick = std::chrono::milliseconds(ZoneConst::DOT_TICK_MS);
    for (Dot& d : dots_)
    {
        int32_t damage = 0;
        while (d.nextTickAt <= now && d.nextTickAt <= d.endAt)
        {
            damage += d.damagePerTick;
            d.nextTickAt += tick;
        }
        if (damage > 0)
            dotHits.push_back({d.casterID, damage});
    }

    // 마지막 틱까지 끝난 도트 제거
    dots_.erase(
        std::remove_if(dots_.begin(), dots_.end(), [&](const Dot& d) { return d.nextTickAt > d.endAt; }), dots_.end());

    return statChanged;
}

bool StatusEffects::Clear(std::vector<StatModifier>* removed)
{
    const bool hasStats = !stats_.empty();
    if (removed)
    {
        for (const StatEffect& e : stats_)
        {
            removed->push_back(e.mod);
        }
    }
    stats_.clear();
    dots_.clear();
    return hasStats;
}

// 버프 / 디버프 리스트 out으로 전달
void StatusEffects::CollectModifiers(std::vector<StatModifier>& out) const
{
    for (const StatEffect& e : stats_)
        out.push_back(e.mod);
}
