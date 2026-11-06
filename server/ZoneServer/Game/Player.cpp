#include "pch.h"
#include "Player.h"

#include "Template/LevelTemplate.h"
#include "Template/StatTemplate.h"
#include "ServerCore/Template/CharacterTemplate.h"

Player::Player(
    uint64_t playerID,
    std::string name,
    uint32_t charID,
    ClassType classType,
    GenderType gender,
    float x,
    float y,
    float z,
    int hp,
    int mp,
    uint64_t exp,
    uint64_t gold,
    uint16_t level)
    : playerID_(playerID), name_(name), charID_(charID), classType_(classType), gender_(gender), x_(x), y_(y), z_(z),
      hp_(hp), mp_(mp), exp_(exp), gold_(gold), level_(level)
{
    ApplyStat();
    hp_ = std::min(hp_, maxHp_);
    mp_ = std::min(mp_, maxMp_);

    lastSaved_ = CurrentState();
}

void Player::SetPosition(float x, float y, float z)
{
    x_ = x;
    y_ = y;
    z_ = z;
}

bool Player::TryMove(float x, float y, float z, float maxSpeed)
{
    // Nan / infinite 거부
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        return false;

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - lastMoveTime_).count();
    lastMoveTime_ = now;
    // 지난 호출 이후 흐른 시간만큼 최대 이동 가능 거리 계산
    moveBudget_ = std::min(moveBudget_ + elapsed * maxSpeed, maxSpeed * 1.0f);

    float dx = x - x_, dy = y - y_, dz = z - z_;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    // 최대 이동 가능 거리 초과
    if (dist > moveBudget_)
        return false;

    moveBudget_ -= dist;
    SetPosition(x, y, z);
    return true;
}

bool Player::CheckSkillCost(SkillCostType type, int32_t cost) const
{
    switch (type)
    {
        case SkillCostType::None:
            return true;
        case SkillCostType::Hp:
            return hp_ > cost;
        case SkillCostType::Mp:
            return mp_ >= cost;
        default:
            return false;
    }
}

bool Player::Regen(int32_t hpAmount, int32_t mpAmount)
{
    if (IsDead())
        return false;

    // 회복 불가 스킬 사용 중이면 예외 처리

    const int oldHp = hp_, oldMp = mp_;
    hp_ = std::min(hp_ + hpAmount, maxHp_);
    mp_ = std::min(mp_ + mpAmount, maxMp_);
    return hp_ != oldHp || mp_ != oldMp;
}

bool Player::AddExp(uint64_t exp)
{
    exp_ += exp;

    // 누적값으로 계산. 최대 레벨이면 LevelFromExp가 MaxLevel에서 멈추므로 아래에서 false
    const uint16_t newLevel = LevelTemplate::Instance().LevelFromExp(exp_);
    if (newLevel <= level_)
        return false;

    level_ = newLevel;
    ApplyStat();
    hp_ = maxHp_;
    mp_ = maxMp_;
    return true;
}

uint64_t Player::AddGold(uint64_t amount)
{
    uint64_t space = gold_ >= Game::MAX_GOLD ? 0 : Game::MAX_GOLD - gold_;
    // min(획득량, 최대 획득 가능량)
    uint64_t addGold = std::min(amount, space);
    gold_ += addGold;
    return addGold;
}

void Player::SpendSkillCost(SkillCostType type, int32_t cost)
{
    switch (type)
    {
        case SkillCostType::Hp:
            hp_ -= cost;
            break;

        case SkillCostType::Mp:
            mp_ -= cost;
            break;
    }
}

void Player::ApplyStat()
{
    const StatData* s = StatTemplate::Instance().Get(classType_, level_);
    if (!s)
        return;

    maxHp_ = s->maxHp;
    maxMp_ = s->maxMp;
    damage_ = s->damage;
}

bool Player::TrySpendSkill(SkillCostType type, int32_t cost)
{
    // 차감 가능 여부 체크
    if (!CheckSkillCost(type, cost))
        return false;

    // 차감 처리
    SpendSkillCost(type, cost);

    return true;
}
