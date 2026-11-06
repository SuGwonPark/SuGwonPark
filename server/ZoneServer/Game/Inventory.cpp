#include "pch.h"
#include "Game/Inventory.h"
#include "Game/ItemUID.h"
#include "Template/ItemTemplate.h"

bool Inventory::LoadSlot(uint16_t slot, uint64_t itemUID, uint32_t itemID, uint32_t count)
{
    if (slot >= SLOT_COUNT || !slots_[slot].IsEmpty())
        return false;

    const ItemData* data = ItemTemplate::Instance().GetData(itemID);
    if (!data || count == 0 || count > data->maxStack)
        return false;

    slots_[slot] = {itemUID, itemID, count};
    return true;
}

// 인벤토리 공간 확인용
bool Inventory::CanAdd(uint32_t itemID, uint32_t count) const
{
    const ItemData* data = ItemTemplate::Instance().GetData(itemID);
    if (!data || count == 0)
        return false;

    uint64_t space = 0;
    for (const InventoryItemData& s : slots_)
    {
        if (s.IsEmpty())
            space += data->maxStack;
        else if (s.itemID == itemID)
            space += data->maxStack - s.count;

        if (space >= count)
            return true;
    }

    return false;
}

// 아이템 추가 (changed = 바뀐 슬롯 번호)
uint32_t Inventory::Add(uint32_t itemID, uint32_t count, std::vector<uint16_t>& changed)
{
    const ItemData* data = ItemTemplate::Instance().GetData(itemID);
    if (!data)
        return count;

    // 동일 아이템 슬롯에 카운트 증가
    for (uint16_t i = 0; i < SLOT_COUNT && count > 0; ++i)
    {
        InventoryItemData& s = slots_[i];
        if (s.itemID != itemID || s.count >= data->maxStack)
            continue;

        const uint32_t put = std::min(count, data->maxStack - s.count);
        s.count += put;
        count -= put;
        changed.push_back(i);
    }

    // 빈 슬롯에 아이템 세팅
    for (uint16_t i = 0; i < SLOT_COUNT && count > 0; ++i)
    {
        InventoryItemData& s = slots_[i];
        if (!s.IsEmpty())
            continue;

        const uint32_t put = std::min(count, data->maxStack);
        s = {ItemUID::Next(), itemID, put};
        count -= put;
        changed.push_back(i);
    }

    return count;
}

void Inventory::CollectAll(std::vector<InventorySlotInfo>& out) const
{
    for (uint16_t i = 0; i < SLOT_COUNT; ++i)
    {
        const InventoryItemData& s = slots_[i];
        if (!s.IsEmpty())
        {
            out.push_back({i, s.itemUID, s.itemID, s.count});
        }
    }
}

// 원하는 인벤 슬롯 정보만 받아옴
void Inventory::Collect(const std::vector<uint16_t>& slots, std::vector<InventorySlotInfo>& out) const
{
    for (uint16_t i :slots)
    {
        const InventoryItemData& s = slots_[i];
        out.push_back({i, s.itemUID, s.itemID, s.count});
    }
}
