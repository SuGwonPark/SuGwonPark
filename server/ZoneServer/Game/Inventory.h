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

    void CollectAll(std::vector<InventorySlotInfo>& out) const;
    void Collect(const std::vector<uint16_t>& slots, std::vector<InventorySlotInfo>& out) const;

  private:
    std::array<InventoryItemData, SLOT_COUNT> slots_{};
};
