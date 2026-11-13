#pragma once
#include <cstdint>
#include <vector>
#include "ServerCore/Protocol/VarEnum.h"

struct ItemData
{
    uint32_t itemID;
    ItemType itemType;
    EquipSlotType equipSlotType; // 슬롯 타입
    GradeType grade;           // 등급
    ClassType classType;       // 클래스 타입
    uint16_t requiredLevel;    // 착용 제한
    uint32_t maxStack;         // 최대 스택 수
    uint32_t sellPrice;        // 판매 가격
    uint32_t optionGroupID;    // 옵션 그룹
    uint32_t subOptionGroupID; // 서브 옵션 그룹
    uint32_t subOptionCount;   // 서브 옵션 개수
    uint32_t useSkillID;       // 사용 시 발동되는 스킬. 없으면 0 (포션, 스크롤)

    bool IsStackable() const
    {
        return maxStack > 1;
    }
};

// Data/Level.csv : ID,Level,Exp  (Exp = 그 레벨이 되기 위한 "누적" 경험치)
class ItemTemplate
{
  public:
    static ItemTemplate& Instance()
    {
        static ItemTemplate inst;
        return inst;
    }

    bool Load();

    bool Validate() const;
    const ItemData* GetData(uint32_t itemID) const;
 

  private:
    // requiredExp_[i] = (i+1)레벨이 되기 위한 누적 경험치. [0]은 항상 0
    std::unordered_map<uint32_t, ItemData> data_;
};