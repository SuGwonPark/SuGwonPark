#include "pch.h"
#include "Game/Inventory.h"
#include "Game/ItemUID.h"
#include "Template/ItemTemplate.h"
#include "Template/ItemOptionTemplate.h"
#include "Template/ItemSubOptionTemplate.h"

// 새 장비 옵션 굴리기: 기본 옵션(그룹 전체) + 추가 옵션 (subOptionCount) 랜덤
static void RollOptions(const ItemData& data, std::vector<ItemOptionInfo>& out)
{
    if (data.optionGroupID != 0)
        ItemOptionTemplate::Instance().GetGroup(data.optionGroupID, out);
    if (data.subOptionGroupID != 0)
        ItemSubOptionTemplate::Instance().Roll(data.subOptionGroupID, data.subOptionCount, out);
}

InventorySlotInfo ToSlotInfo(uint16_t slot, const InventoryItemData& s)
{
    InventorySlotInfo info{};
    info.itemUID = s.itemUID;
    info.itemID = s.itemID;
    info.slot = slot;
    info.count = s.count;
    info.optionCount = static_cast<uint8_t>(std::min<size_t>(s.options.size(), Game::MAX_ITEM_OPTIONS));
    for (uint8_t i = 0; i < info.optionCount; ++i)
    {
        info.options[i] = s.options[i];
    }

    return info;
}

EquipSlotInfo ToEquipInfo(EquipSlotType est, const InventoryItemData& e)
{
    EquipSlotInfo info{};
    info.equipSlot = est;
    info.itemUID = e.itemUID;
    info.itemID = e.itemID;
    info.optionCount = static_cast<uint8_t>(std::min<size_t>(e.options.size(), Game::MAX_ITEM_OPTIONS));
    for (uint8_t i = 0; i < info.optionCount; ++i)
        info.options[i] = e.options[i];
    return info;
}


bool Inventory::LoadSlot(
    uint16_t slot, uint64_t itemUID, uint32_t itemID, uint32_t count, std::vector<ItemOptionInfo> options)
{
    if (slot >= SLOT_COUNT || !slots_[slot].IsEmpty())
        return false;

    const ItemData* data = ItemTemplate::Instance().GetData(itemID);
    if (!data || count == 0 || count > data->maxStack)
        return false;

    if (options.size() > Game::MAX_ITEM_OPTIONS)
        return false;

    slots_[slot] = {itemUID, itemID, count, std::move(options)};
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
        s = {ItemUID::Next(), itemID, put, {}};

        // 장비 아이템만
        if (data->itemType == ItemType::Equip)
            RollOptions(*data, s.options);

        count -= put;
        changed.push_back(i);
    }

    return count;
}

int16_t Inventory::FindSmallestStack(uint32_t itemID) const
{
    int16_t best = -1;
    for (uint16_t i = 0; i < SLOT_COUNT; ++i)
    {
        const InventoryItemData& s = slots_[i];
        if (s.IsEmpty() || s.itemID != itemID)
            continue;

        // 저장해둔 아이템보다 갯수가 많으면 넘김 (갯수가 적은것부터 소모하기 위해 찾음)
        if (best < 0 || s.count < slots_[best].count)
            best = i;
    }
    return best;
}

bool Inventory::Remove(uint16_t slot, uint32_t count, std::vector<uint64_t>& removed)
{
    if (slot >= SLOT_COUNT || count <= 0)
        return false;

    InventoryItemData& data = slots_[slot];
    if (data.IsEmpty() || data.count < count)
        return false;

    data.count -= count;

    // 아이템 0개로 slot 데이터를 초기화
    if (data.count == 0)
    {
        removed.push_back(data.itemUID);
        data = InventoryItemData{};
    }

    return true;
}

void Inventory::CollectAll(std::vector<InventorySlotInfo>& out) const
{
    for (uint16_t i = 0; i < SLOT_COUNT; ++i)
    {
        const InventoryItemData& s = slots_[i];
        if (!s.IsEmpty())
        {
            out.push_back(ToSlotInfo(i, s));
        }
    }
}

// 원하는 인벤 슬롯 정보만 받아옴
void Inventory::Collect(const std::vector<uint16_t>& slots, std::vector<InventorySlotInfo>& out) const
{
    for (uint16_t i : slots)
    {
        const InventoryItemData& s = slots_[i];
        out.push_back(ToSlotInfo(i, s));
    }
}

int32_t Inventory::FindSlot(uint64_t itemUID) const
{
    if (itemUID == 0)
        return -1;

    for (uint16_t i = 0; i < SLOT_COUNT; ++i)
    {
        if (slots_[i].itemUID == itemUID)
            return i;
    }

    return -1;
}

int32_t Inventory::FindEmpty() const
{
    for (uint16_t i = 0; i < SLOT_COUNT; ++i)
    {
        if (slots_[i].IsEmpty())
            return i;
    }

    return -1;
}

InventoryItemData Inventory::Take(uint16_t slot)
{
    if (slot >= SLOT_COUNT)
        return {};

    InventoryItemData out = std::move(slots_[slot]);
    slots_[slot] = InventoryItemData{};
    return out;
}

bool Inventory::Put(uint16_t slot, InventoryItemData item)
{
    if (slot >= SLOT_COUNT || !slots_[slot].IsEmpty() || item.IsEmpty())
        return false;

    slots_[slot] = std::move(item);
    return true;
}
