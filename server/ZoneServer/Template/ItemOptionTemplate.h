#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ServerCore/Protocol/VarEnum.h"

// 굴린 옵션 1줄 = 장비 1개에 실제로 붙는 값 (나중에 DB item_option에 저장)
struct RolledOption
{
    StatType statType;
    ValueType valueType;
    int32_t value;
};

//  - 장비 기본 옵션. 그룹의 모든 줄이 붙고 값만 Min~Max에서 굴린다
struct ItemOptionData
{
    uint32_t id;
    uint32_t groupID;
    StatType statType;
    ValueType valueType;
    int32_t minValue;
    int32_t maxValue;
};

class ItemOptionTemplate
{
  public:
    static ItemOptionTemplate& Instance()
    {
        static ItemOptionTemplate inst;
        return inst;
    }

    bool Load();

    bool HasGroup(uint32_t groupID) const
    {
        return groups_.count(groupID) > 0;
    }

    // 그룹의 모든 옵션을 굴려서 out 뒤에 추가
    void GetGroup(uint32_t groupID, std::vector<RolledOption>& out) const;

  private:
    std::unordered_map<uint32_t, std::vector<ItemOptionData>> groups_; 
};