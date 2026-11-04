#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Template/ItemOptionTemplate.h"

// Data/ItemSubOption.csv : ID,GroupID,StatType,ValueType,MinValue,MaxValue,Rate
//  - 장비 추가 옵션. 그룹에서 Rate 가중치로 SubOptionCount개를 "중복 없이" 뽑는다
//  - Rate는 가중치 (합이 RewardMaxRate일 필요 없음, 꽝 없음)
struct ItemSubOptionData
{
    uint32_t id;
    uint32_t groupID;
    StatType statType;
    ValueType valueType;
    int32_t minValue;
    int32_t maxValue;
    uint32_t rate;
};

class ItemSubOptionTemplate
{
  public:
    static ItemSubOptionTemplate& Instance()
    {
        static ItemSubOptionTemplate inst;
        return inst;
    }

    bool Load();

    // 그룹의 줄 수 (SubOptionCount 검증용). 없는 그룹이면 0
    size_t GroupSize(uint32_t groupID) const
    {
        auto it = groups_.find(groupID);
        return it == groups_.end() ? 0 : it->second.rows.size();
    }

    // count개를 중복 없이 뽑아서 out 뒤에 추가
    void Roll(uint32_t groupID, uint32_t count, std::vector<RolledOption>& out) const;

  private:
    struct Group
    {
        std::vector<ItemSubOptionData> rows;
        std::vector<int> weights; // rows[i].rate (Random::WeightedIndex용)
    };
    std::unordered_map<uint32_t, Group> groups_;
};