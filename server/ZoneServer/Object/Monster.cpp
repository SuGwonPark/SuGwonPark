#include "pch.h"
#include "Object/Monster.h"
#include "World/ZoneConstants.h"
#include "Combat/Combat.h"
#include "Util/MathUtil.h"

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
    return Math::DistSq3D(x_, y_, z_, x, y, z);
}

// 타겟 남은 거리 체크
bool Monster::MoveToward(float tx, float ty, float tz, float step, float stopDist)
{
    float dx = tx - x_, dy = ty - y_, dz = tz - z_;
    // 몬스터 위치에서 내 위치까지의 거리
    float dist = Math::Length3D(dx, dy, dz);

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
        auto it = std::find_if(contributions_.begin(), contributions_.end(), [&](const Contribution& c) {
            return c.playerID == attackerID;
        });
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

// 버프 / 디버프
EffectResult Monster::AddStatEffect(StatType stat, ValueType valueType, int32_t value, Clock::time_point endAt)
{
    const EffectResult r = effects_.AddStat(stat, valueType, value, endAt);
    if (r == EffectResult::Applied)
        RecalcStat();

    return r;
}

// 도트 추가 (도트는 중첩 가능하도록 추가)
EffectResult Monster::AddDot(
    uint64_t casterID, DotType dotType, int32_t damagePerTick, Clock::time_point now, Clock::time_point endAt)
{
    return effects_.AddDot(casterID, dotType, damagePerTick, now, endAt);
}

int32_t Monster::UpdateEffects(Clock::time_point now)
{
    if (effects_.Empty())
        return 0;

    std::vector<StatModifier> expired;
    std::vector<DotHit> dotHit;
    if (effects_.Update(now, expired, dotHit))
        RecalcStat();

    // 도트 데미지
    int32_t total = 0;
    for (const DotHit& h : dotHit)
        total += TakeDamage(h.damage, h.casterID);
    return total;
}

void Monster::ClearEffects()
{
    effects_.Clear();
    stat_ = data_->stat;
}

// 디버프는 감소량으로 저장되어 있으므로 음수로 전달
void Monster::RecalcStat()
{
    std::vector<StatModifier> mods;
    effects_.CollectModifiers(mods);

    stat_ = Combat::CalcStat(data_->stat, mods);
    hp_ = std::min(hp_, stat_.maxHp);
}
