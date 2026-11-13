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
    const StatData* data = StatTemplate::Instance().Get(classType_, level_);
    if (!data)
        return;

    CombatStat base{};
    base.maxHp = data->maxHp;
    base.maxMp = data->maxMp;
    base.damage = data->damage;
    base.critRate = data->critRate;
    base.critDamage = data->critDamage;
    base.moveSpeed = ZoneConst::MAX_MOVE_SPEED;
    base.hpRegen = ZoneConst::PLAYER_HP_REGEN;
    base.mpRegen = ZoneConst::PLAYER_MP_REGEN;
    
    std::vector<StatModifier> mods;
    for (const InventoryItemData& e : equipment_)
        for (const ItemOptionInfo& o : e.options)
            mods.push_back({o.statType, o.valueType, o.value});

    stat_ = Combat::CalcStat(base, mods);
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
    return std::chrono::milliseconds(Combat::AttackCooldownMs(ZoneConst::PLAYER_ATTACK_COOLDOWN_MS, stat_));
}

bool Player::TryUseAttack(std::chrono::steady_clock::time_point now)
{
    if (now - lastAttackTime_ < GetAttackCooldown())
        return false;
    lastAttackTime_ = now;
    return true;
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
