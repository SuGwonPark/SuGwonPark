#include "pch.h"
#include "Player.h"

#include "Game/ZoneConstants.h"

#include "Template/LevelTemplate.h"
#include "Template/StatTemplate.h"
#include "Template/ItemTemplate.h"
#include "Template/SkillTemplate.h"

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
    CalcStat();
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
    hp_ = std::min(hp_ + hpAmount, stat_.maxHp);
    mp_ = std::min(mp_ + mpAmount, stat_.maxMp);
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
    CalcStat();
    hp_ = stat_.maxHp;
    mp_ = stat_.maxMp;
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

void Player::CalcStat()
{
    const StatData* base = StatTemplate::Instance().Get(classType_, level_);
    if (!base)
        return;

    // 스탯별 Flat / Percent 합
    constexpr size_t STAT_COUNT = static_cast<size_t>(StatType::Max);
    int64_t flat[STAT_COUNT] = {};
    int64_t pct[STAT_COUNT] = {};

    // 장비 스탯 옵션 총 합산
    for (const InventoryItemData& e : equipment_)
    {
        for (const ItemOptionInfo& o : e.options)
        {
            const size_t i = static_cast<size_t>(o.statType);
            if (i >= STAT_COUNT)
                continue;
            auto& pick = o.valueType == ValueType::Percent ? pct : flat;
            pick[i] += o.value;
        }
    }

    // 일반 스탯: (기본 + flat) * (1 + percent)
    auto normal = [&](StatType t, double baseValue) {
        const size_t i = static_cast<size_t>(t);
        return (baseValue + flat[i]) * (1.0 + static_cast<double>(pct[i] / BuffMaxPercent));
    };

    // 확률 / 배율 스탯 (값 자체가 만분율이라 그냥 더함)
    auto rate = [&](StatType t, int32_t baseValue) {
        const size_t i = static_cast<size_t>(t);
        return static_cast<int32_t>(baseValue + flat[i] + pct[i]);
    };

    auto toInt = [](double v) { return static_cast<int32_t>(std::lround(v)); };
    const int32_t maxRate = static_cast<int32_t>(BuffMaxPercent);

    stat_.maxHp = std::max(1, toInt(normal(StatType::Hp, base->maxHp)));
    stat_.maxMp = std::max(0, toInt(normal(StatType::Mp, base->maxMp)));
    stat_.damage = std::max(1, toInt(normal(StatType::Damage, base->damage)));
    stat_.defense = std::max(0, toInt(normal(StatType::Defense, 0)));
    stat_.critRate = std::clamp(rate(StatType::CritRate, base->critRate), 0, maxRate);  // Stat.csv
    stat_.critDamage = std::max(maxRate, rate(StatType::CritDamage, base->critDamage)); // 기본 100%
    stat_.attackSpeed = std::max(0, rate(StatType::AttackSpeed, 0));
    stat_.moveSpeed = static_cast<float>(normal(StatType::MoveSpeed, ZoneConst::MAX_MOVE_SPEED));
    stat_.hpRegen = std::max(0, toInt(normal(StatType::HpRegen, ZoneConst::PLAYER_HP_REGEN)));
    stat_.mpRegen = std::max(0, toInt(normal(StatType::MpRegen, ZoneConst::PLAYER_MP_REGEN)));
}

void Player::RecalcStat()
{
    CalcStat();
    hp_ = std::min(hp_, stat_.maxHp);
    mp_ = std::min(mp_, stat_.maxMp);
}

void Player::SwapEquip(uint16_t slotID, EquipSlotType slot, InventoryItemData in)
{
    InventoryItemData& e = equipment_[static_cast<size_t>(slot)];
    InventoryItemData old = std::move(e);
    e = std::move(in);

    // 장착중이던 장비 인벤토리로 이동
    if (!old.IsEmpty())
        inventory_.Put(slotID, std::move(old));

}

// DB 데이터 세팅시 사용
bool Player::LoadEquip(uint16_t equipSlot, uint64_t itemUID, uint32_t itemID, std::vector<ItemOptionInfo> options)
{
    if (equipSlot == 0 || equipSlot >= static_cast<size_t>(EquipSlotType::Max))
        return false;

    InventoryItemData& e = equipment_[equipSlot];
    if (!e.IsEmpty())
        return false;

    const ItemData* itemData = ItemTemplate::Instance().GetData(itemID);
    if (!itemData || itemData->itemType != ItemType::Equip ||
        static_cast<uint32_t>(itemData->equipSlotType) != equipSlot)
        return false;

    e = {itemUID, itemID, 1, std::move(options)};
    return true;
}

std::chrono::milliseconds Player::GetAttackCooldown() const
{
    const double ms = static_cast<int64_t>(std::lround(
        ZoneConst::PLAYER_ATTACK_COOLDOWN_MS * static_cast<double>(BuffMaxPercent) /
        (BuffMaxPercent + stat_.attackSpeed)));
    return std::chrono::milliseconds(std::lround(ms));
}

bool Player::TryUseAttack(std::chrono::steady_clock::time_point now)
{
    if (now - lastAttackTime_ < GetAttackCooldown())
        return false;
    lastAttackTime_ = now;
    return true;
}

int32_t Player::ApplyDefense(int32_t rawDamage) const
{
    return std::max(1, static_cast<int32_t>(std::lround(rawDamage * 100.0 / (100.0 + stat_.defense))));
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

bool Player::TrySpendSkill(SkillCostType type, int32_t cost)
{
    // 차감 가능 여부 체크
    if (!CheckSkillCost(type, cost))
        return false;

    // 차감 처리
    SpendSkillCost(type, cost);

    return true;
}
