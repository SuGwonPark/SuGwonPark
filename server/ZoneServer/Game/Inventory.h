#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "ServerCore/Protocol/ProtocolTypes.h"

// itemID == 0 이면 빈칸
struct InventoryItemData
{
    uint64_t itemUID = 0;
    uint32_t itemID = 0;
    uint32_t count = 0;
    std::vector<ItemOptionInfo> options; // 장비만

    bool IsEmpty() const
    {
        return itemID == 0;
    }
};

InventorySlotInfo ToSlotInfo(uint16_t slot, const InventoryItemData& s);
EquipSlotInfo ToEquipInfo(EquipSlotType est, const InventoryItemData& e);

class Inventory
{
  public:
    static constexpr uint16_t SLOT_COUNT = Game::INVENTORY_SIZE;

    // 인벤토리 슬롯 정보 요청
    bool LoadSlot(
        uint16_t slot, uint64_t itemUID, uint32_t itemID, uint32_t count, std::vector<ItemOptionInfo> options);

    // 슬롯에 아이템 저장 가능 여부 확인
    bool CanAdd(uint32_t itemID, uint32_t count) const;

    // 슬롯에 아이템 저장
    uint32_t Add(uint32_t itemID, uint32_t count, std::vector<uint16_t>& changed);

    // 슬롯 아이템 정보
    const InventoryItemData& GetSlot(uint16_t slot) const
    {
        return slots_[slot];
    }

    // 소모 아이템 찾기
    int16_t FindSmallestStack(uint32_t itemID) const;

    bool Remove(uint16_t slot, uint32_t count, std::vector<uint64_t>& removed);

    void CollectAll(std::vector<InventorySlotInfo>& out) const;
    void Collect(const std::vector<uint16_t>& slots, std::vector<InventorySlotInfo>& out) const;

    // 아이템 슬롯 찾기
    int32_t FindSlot(uint64_t itemUID) const;
    int32_t FindEmpty() const;
    // 아이템 장착시 인벤토리 비우기
    InventoryItemData Take(uint16_t slot);
    // 아이템 해제시 인벤토리에 넣기
    bool Put(uint16_t slot, InventoryItemData item);

  private:
    std::array<InventoryItemData, SLOT_COUNT> slots_{};
};
