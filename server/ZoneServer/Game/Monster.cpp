#include "pch.h"
#include "Monster.h"
#include "ZoneConstants.h"

Monster::Monster(uint64_t monsterUID, const MonsterData* data, float x, float y, float z)
    : monsterUID_(monsterUID), data_(data), stat_(data->stat), x_(x), y_(y), z_(z), spawnX_(x), spawnY_(y), spawnZ_(z),
      hp_(data->stat.maxHp)
{
}

void Monster::Respawn()
{
    x_ = spawnX_;
    y_ = spawnY_;
    z_ = spawnZ_;
    SetIdle();
    ClearEffects(); // 스탯을 원래대로 돌린 다음 HP를 채움
    hp_ = stat_.maxHp;
    contributions_.clear();
}

// 제곱 사용 (대상 범위도 제곱해야함)
float Monster::DistSqTo(float x, float y, float z) const
{
    float dx = x - x_, dy = y - y_, dz = z - z_;
    return dx * dx + dy * dy + dz * dz;
}

// 타겟 남은 거리 체크
bool Monster::MoveToward(float tx, float ty, float tz, float step, float stopDist)
{
    float dx = tx - x_, dy = ty - y_, dz = tz - z_;
    // 몬스터 위치에서 내 위치까지의 거리
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    // 남은 거리
    float remain = dist - stopDist;
    if (remain <= 0.f || step <= 0.f)
        return false;

    // 목표물 앞 까지만 이동
    float move = std::min(step, remain);
    float ratio = move / dist;
    x_ += dx * ratio;
    y_ += dy * ratio;
    z_ += dz * ratio;

    return true;
}

// 가한 데미지
int32_t Monster::TakeDamage(int32_t damage, uint64_t attackerID)
{
    if (IsDead() || damage <= 0)
        return 0;
    const int32_t applied = std::min(damage, hp_);
    hp_ -= applied;

    // 실제 깍인 양 기여도
    if (attackerID > 0 && applied > 0)
    {
        auto it = std::find_if(contributions_.begin(), contributions_.end(), [&](const Contribution& c){ return c.playerID == attackerID; });
        if (it != contributions_.end())
            // 기존 데미지 합산
            it->damage += applied;
        else
            // 신규 추가
            contributions_.push_back({attackerID, applied});
    }


    return applied;
}

// 사망 
void Monster::Die(Clock::time_point now)
{
    hp_ = 0;
    state_ = MonsterState::Dead;
    targetPlayerID_ = 0;
    // 리스폰 시간 세팅 (현재시간 + 리스폰 시간 이후 생성)
    respawnAt_ = now + std::chrono::seconds(data_->respawnSec);
    ClearEffects();
}

// 초기화
bool Monster::ResetHp()
{
    if (hp_ == stat_.maxHp)
        return false;
    hp_ = stat_.maxHp;
    return true;
}

// 디버프 추가
void Monster::AddDebuff(StatType stat, ValueType valueType, int32_t value, Clock::time_point endAt)
{
    if (value <= 0)
        return;

    auto it = std::find_if(debuffs_.begin(), debuffs_.end(), [&](const Debuff& d) { return d.stat == stat && d.valueType == valueType; });

    if (it == debuffs_.end())
        debuffs_.push_back({stat, valueType, value,endAt});
    else if (it->value < value)
        *it = {stat, valueType, value, endAt};
    else
    {
        if (it->value == value)
            it->endAt = std::max(it->endAt, endAt);
        return;
    }

    RecalcStat();
}

// 도트 추가 (도트는 중첩 가능하도록 추가)
void Monster::AddDot(
    uint64_t casterID, DotType dotType, int32_t damagePerTick, Clock::time_point now, Clock::time_point endAt)
{
    if (damagePerTick <= 0)
        return;

    dots_.push_back({casterID, dotType, damagePerTick, now + std::chrono::milliseconds(ZoneConst::DOT_TICK_MS), endAt});
}

int32_t Monster::UpdateEffects(Clock::time_point now)
{
    // 끝난 디버프 제거. 하나라도 빠졌으면 스탯 다시 계산
    const size_t before = debuffs_.size();
    debuffs_.erase(
        std::remove_if(debuffs_.begin(), debuffs_.end(), [&](const Debuff& d) { return now >= d.endAt; }),
        debuffs_.end());
    if (debuffs_.size() != before)
        RecalcStat();

    int32_t total = 0;
    const auto tick = std::chrono::milliseconds(ZoneConst::DOT_TICK_MS);
    for (Dot& d : dots_)
    {
        // 도트 데미지 
        while (d.nextTickAt <= now && d.nextTickAt <= d.endAt)
        {
            total += TakeDamage(d.damagePerTick, d.casterID);
            d.nextTickAt += tick;
        }
    }

    // 마지막 틱까지 끝난 도트 제거
    dots_.erase(
        std::remove_if(dots_.begin(), dots_.end(), [&](const Dot& d) { return d.nextTickAt > d.endAt; }), dots_.end());

    return total;
}

void Monster::ClearEffects()
{
    debuffs_.clear();
    dots_.clear();
    stat_ = data_->stat;
}

// 디버프는 감소량으로 저장되어 있으므로 음수로 전달
void Monster::RecalcStat() {
    std::vector<StatModifier> mods;
    mods.reserve(debuffs_.size());
    for (const Debuff& d : debuffs_)
        mods.push_back({d.stat, d.valueType, -d.value});

    stat_ = Combat::CalcStat(data_->stat, mods);
    hp_ = std::min(hp_, stat_.maxHp);
}
