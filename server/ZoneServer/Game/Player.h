#pragma once
#include "ServerCore/Protocol/VarEnum.h"
#include "Game/Inventory.h"

class Player
{
  private:
    uint64_t userUID = 0; // 임시
    uint64_t playerID_;
    uint32_t charID_ = 0;
    ClassType classType_ = ClassType::None;
    GenderType gender_ = GenderType::None;
    uint16_t level_ = 1;
    std::string name_;
    float x_;
    float y_;
    float z_;
    int hp_;
    int mp_;
    uint64_t exp_;
    uint64_t gold_ = 0;
    Inventory inventory_;
    StatInfo stat_{};                                                   // 최종 스탯 정보 (기본 스탯 + 장비)
    std::array<InventoryItemData, Game::EQUIP_SLOT_COUNT> equipment_{}; // 장착 아이템 정보

    uint64_t sessionID_ = 0;

    // 마지막 이동 시간
    std::chrono::steady_clock::time_point lastMoveTime_ = std::chrono::steady_clock::now();
    float moveBudget_ = 0.f;

    // 마지막 공격 시간
    std::chrono::steady_clock::time_point lastAttackTime_{};

    // 스킬별 쿨타임
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> skillReadyAt_;

    struct SavedState
    {
        uint64_t exp;
        uint64_t gold;
        float x, y, z;
        int hp, mp;
        bool operator==(const SavedState& o) const
        {
            return exp == o.exp && gold == o.gold && x == o.x && y == o.y && z == o.z && hp == o.hp && mp == o.mp;
        }
    };
    SavedState lastSaved_{};
    SavedState CurrentState() const
    {
        return {exp_, gold_, x_, y_, z_, hp_, mp_};
    }

  private:
    // 스킬 사용 처리
    void SpendSkillCost(SkillCostType type, int32_t cost);
        

  public:
    Player(
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
        uint16_t level);

    void SetPosition(float x, float y, float z);
    bool TryMove(float x, float y, float z, float maxSpeed);

    // 스킬 사용
    bool TrySpendSkill(SkillCostType type, int32_t cost);
    // 스킬 사용 가능 여부 체크
    bool CheckSkillCost(SkillCostType type, int32_t cost) const;

    // 자동 회복
    bool Regen(int32_t hpAmount, int32_t mpAmount);

    bool AddExp(uint64_t exp);

    uint64_t AddGold(uint64_t amount);

    // 기본 스탯 + 장비 스탯
    void CalcStat();

    // 스탯 다시 계산
    void RecalcStat();

    // 장착 칸에 in을 넣고 원래 있던 장비를 인벤토리로 돌려줌
    void SwapEquip(uint16_t slotID, EquipSlotType slot, InventoryItemData in);

    // DB 갱신
    bool LoadEquip(uint16_t equipSlot, uint64_t itemUID, uint32_t itemID, std::vector<ItemOptionInfo> options);

    std::chrono::milliseconds GetAttackCooldown() const;
    bool TryUseAttack(std::chrono::steady_clock::time_point now);

    // 받은 공격력에 내 방어력을 적용한 데미지 (최소 1)
    int32_t ApplyDefense(int32_t rawDamage) const;

    uint64_t GetExp() const
    {
        return exp_;
    }

    uint64_t GetGold() const
    {
        return gold_;
    }

    Inventory& GetInventory()
    {
        return inventory_;
    }

    const Inventory& GetInventory() const
    {
        return inventory_;
    }

    uint64_t GetPlayerID() const
    {
        return playerID_;
    }

    GenderType GetGender() const
    {
        return gender_;
    }

    uint64_t GetSessionID() const
    {
        return sessionID_;
    }
    void SetSessionID(uint64_t sessionID)
    {
        sessionID_ = sessionID;
    }

    float GetX() const
    {
        return x_;
    }
    float GetY() const
    {
        return y_;
    }
    float GetZ() const
    {
        return z_;
    }

    int GetMaxHp() const
    {
        return stat_.maxHp;
    }
    int GetMaxMp() const
    {
        return stat_.maxMp;
    }
    int GetHp() const
    {
        return hp_;
    }
    int GetMp() const
    {
        return mp_;
    }

    const std::string& GetName() const
    {
        return name_;
    }

    bool IsDead() const
    {
        return hp_ <= 0;
    }

    ClassType GetClassType() const
    {
        return classType_;
    }

    uint16_t GetLevel() const
    {
        return level_;
    }

    uint32_t GetDamage() const
    {
        return stat_.damage;
    }

    const StatInfo& GetStat() const
    {
        return stat_;
    }

    int32_t TakeDamage(int32_t damage)
    {
        if (IsDead() || damage <= 0)
            return 0;
        const int32_t applied = damage < hp_ ? damage : hp_;
        hp_ -= applied;
        return applied;
    }

    void Revive(float x, float y, float z, int hp)
    {
        hp_ = hp;
        SetPosition(x, y, z);
    }

    // 공격 가능 여부 체크
    bool TryUseAttack(std::chrono::steady_clock::time_point now, std::chrono::milliseconds cooldown)
    {
        if (now - lastAttackTime_ < cooldown)
            return false;
        lastAttackTime_ = now;
        return true;
    }

    // 스킬 사용 가능 여부
    bool IsSkillReady(uint32_t skillID, std::chrono::steady_clock::time_point now)
    {
        auto it = skillReadyAt_.find(skillID);
        return it == skillReadyAt_.end() || now >= it->second;
    }

    // 스킬 쿨타임 적용
    void StartSkillCooldown(uint32_t skillID, std::chrono::steady_clock::time_point now, int32_t cooldownMs)
    {
        auto cooldown = std::chrono::milliseconds(cooldownMs);
        skillReadyAt_[skillID] = now + cooldown;
    }

    // 마지막 저장 상태에서 변경이 있을 경우만 true
    bool ConsumeChanged()
    {
        const SavedState now = CurrentState();
        if (now == lastSaved_)
            return false;
        lastSaved_ = now;
        return true;
    }
    
    // 장착 슬롯의 장비 정보
    const InventoryItemData& GetEquip(EquipSlotType slot) const
    {
        return equipment_[static_cast<size_t>(slot)];
    }
};