#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ServerCore/Protocol/VarEnum.h"
#include "Game/ZoneConstants.h"

// 스킬 효과 (데미지, 버프, 디버프 등)
struct SkillEffectData
{
    uint32_t groupID;
    SkillEffectType effectType; // 효과 타입
    StatType statType = StatType::None;          // 능력치에 영향을 주는 효과면 StatType. 아니면 None
    DotType dotType = DotType::None;            // 도트 스킬이면 DotType. 아니면 None
    ValueType valueType = ValueType::None;        // Flat = 정수, Percent = 만분율 (10000 = 100%)
    int32_t value;
    int32_t durationMs; // 지속 시간
};

class SkillEffectTemplate
{
  public:
    static SkillEffectTemplate& Instance()
    {
        static SkillEffectTemplate inst;
        return inst;
    }

    bool Load();
    bool Validate() const;

    bool HasGroup(uint32_t groupID) const
    {
        return byGroup_.count(groupID) > 0;
    }

    // 그룹의 효과 목록 (CSV에 적힌 순서). 없으면 nullptr
    const std::vector<SkillEffectData>* GetGroup(uint32_t groupID) const;

    size_t Count() const
    {
        return count_;
    }

  private:
    std::unordered_map<uint32_t, std::vector<SkillEffectData>> byGroup_;
    size_t count_ = 0;
};
