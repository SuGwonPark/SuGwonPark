#pragma once
#include <cstdint>
#include <unordered_map>
#include "ServerCore/Protocol/VarEnum.h" 


struct SkillData
{
    uint32_t skillID;
    ClassType classType;   // None = 직업 제한 없음 (아이템 스킬 등 공용)
    TargetType targetType; // 누구를 대상으로 (적 / 아군 / 본인)
    RangeType rangeType;   // 범위 모양 (단일 / 시전자 중심 / 대상 중심)
    float range;           // Single / TargetArea: 대상까지 최대 거리
    float radius;          // SelfArea / TargetArea: 반경
    int32_t cooldownMs;
    int32_t castingMs; // 시전 시간. 0이면 즉시 발동
    SkillCostType costType; // 소모 자원 타입
    int32_t cost;           // 소모 자원 값
    uint32_t effectGroupID; // 스킬 효과
};

class SkillTemplate
{
  public:
    static SkillTemplate& Instance()
    {
        static SkillTemplate inst;
        return inst;
    }

    bool Load();
    bool Validate() const; 

        const SkillData* GetData(ClassType classType, uint32_t skillID) const;

    const std::unordered_map<uint64_t, SkillData>& All() const
    {
        return data_;
    }

  private:
    static uint64_t Key(ClassType classType, uint32_t skillID)
    {
        return (static_cast<uint64_t>(classType) << 32) | skillID;
    }
    std::unordered_map<uint64_t, SkillData> data_;
};
