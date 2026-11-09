#include "pch.h"
#include "Template/ItemTemplate.h"
#include "Template/ItemOptionTemplate.h"
#include "Template/ItemSubOptionTemplate.h"
#include "Template/LevelTemplate.h"
#include "Template/SkillTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"

bool ItemTemplate::Load()
{
    data_.clear();
    return Csv::ForEachRow("Item.csv", [this](const CsvRowView& r) {
        ItemData d{};
        d.itemID = r.U32("ItemID");
        d.itemType = ParseCsvEnum<ItemType>(r.Str("ItemType"), "ItemType", d.itemID);
        d.equipSlot = ParseCsvEnum2<EquipSlot>(r.Str("EquipSlot"), "EquipSlot", d.itemID); // None 허용
        d.grade = ParseCsvEnum<GradeType>(r.Str("Grade"), "Grade", d.itemID);
        d.classType = ParseCsvEnum2<ClassType>(r.Str("ClassType"), "ClassType", d.itemID); // None = 공용
        const uint32_t level = r.U32("RequiredLevel");
        d.maxStack = r.U32("MaxStack");
        d.sellPrice = r.U32("SellPrice");
        d.optionGroupID = r.U32("OptionGroupID");
        d.subOptionGroupID = r.U32("SubOptionGroupID");
        d.subOptionCount = r.U32("SubOptionCount");
        d.useSkillID = r.U32("UseSkillID");

        if (d.itemID == 0)
            throw std::invalid_argument("ID는 1 이상");
        if (level == 0 || level > 0xFFFF)
            throw std::invalid_argument("RequiredLevel은 1 ~ 65535");
        d.requiredLevel = static_cast<uint16_t>(level);
        if (d.maxStack == 0)
            throw std::invalid_argument("MaxStack은 1 이상");
        if (d.subOptionCount > 0 && d.subOptionGroupID == 0)
            throw std::invalid_argument("SubOptionCount가 있으면 SubOptionGroupID가 필요");

        switch (d.itemType)
        {
            case ItemType::Equip:
                // 장비는 강화/옵션이 개별로 달라서 겹칠 수 없다
                if (d.maxStack != 1)
                    throw std::invalid_argument("Equip은 MaxStack 1");
                if (d.equipSlot == EquipSlot::None)
                    throw std::invalid_argument("Equip은 EquipSlot이 필요");
                if (d.useSkillID != 0)
                    throw std::invalid_argument("Equip은 UseSkillID 0");
                break;

            case ItemType::Consumable:
                if (d.equipSlot != EquipSlot::None)
                    throw std::invalid_argument("Consumable은 EquipSlot None");
                if (d.useSkillID == 0)
                    throw std::invalid_argument("Consumable은 UseSkillID가 필요");
                break;

            default: // Material
                if (d.equipSlot != EquipSlot::None)
                    throw std::invalid_argument("Material은 EquipSlot None");
                if (d.useSkillID != 0)
                    throw std::invalid_argument("Material은 UseSkillID 0");
                break;
        }

        if (d.itemType != ItemType::Equip && (d.optionGroupID != 0 || d.subOptionGroupID != 0))
            throw std::invalid_argument("장비가 아니면 OptionGroupID / SubOptionGroupID는 0");

        if (!data_.emplace(d.itemID, d).second)
            throw std::invalid_argument("중복 ID " + r.Str("ID"));
    });
}

bool ItemTemplate::Validate() const
{
    bool ok = true;
    const uint16_t maxLevel = LevelTemplate::Instance().MaxLevel();

    for (const auto& [id, d] : data_)
    {
        if (d.requiredLevel > maxLevel)
        {
            std::cerr << "[Template] Item.csv ID " << id << ": RequiredLevel " << d.requiredLevel << "이 최대 레벨 "
                      << maxLevel << " 초과" << std::endl;
            ok = false;
        }

        // 사용 스킬: 그 직업 스킬 또는 공용(ClassType None) 스킬이어야 함 (GetData가 공용으로 대체 검색)
        if (d.useSkillID != 0 && !SkillTemplate::Instance().GetData(d.classType, d.useSkillID))
        {
            std::cerr << "[Template] Item.csv ID " << id << ": Skill.csv에 없는 UseSkillID " << d.useSkillID
                      << std::endl;
            ok = false;
        }

        // 옵션 그룹이 실제로 있는지
        if (d.optionGroupID != 0 && !ItemOptionTemplate::Instance().HasGroup(d.optionGroupID))
        {
            std::cerr << "[Template] Item.csv ID " << id << ": ItemOption.csv에 없는 OptionGroupID " << d.optionGroupID
                      << std::endl;
            ok = false;
        }

        // 서브 옵션: 그룹이 있어야 하고, 뽑을 개수가 그룹 줄 수 이하 (중복 없이 뽑으니까)
        if (d.subOptionGroupID != 0)
        {
            const size_t size = ItemSubOptionTemplate::Instance().GroupSize(d.subOptionGroupID);
            if (size == 0)
            {
                std::cerr << "[Template] Item.csv ID " << id << ": ItemSubOption.csv에 없는 SubOptionGroupID "
                          << d.subOptionGroupID << std::endl;
                ok = false;
            }
            else if (d.subOptionCount > size)
            {
                std::cerr << "[Template] Item.csv ID " << id << ": SubOptionCount " << d.subOptionCount
                          << "가 그룹 줄 수 " << size << " 초과" << std::endl;
                ok = false;
            }
        }

        // 장비 1개 옵션 수 = 기본 그룹 줄 수 + 추가 옵션 수 ≤ MAX_ITEM_OPTIONS (패킷 고정 배열 크기)
        const size_t optionTotal = ItemOptionTemplate::Instance().GroupSize(d.optionGroupID) + d.subOptionCount;
        if (optionTotal > Game::MAX_ITEM_OPTIONS)
        {
            std::cerr << "[Template] Item.csv ItemID " << id << ": 옵션 " << optionTotal << "개가 최대 "
                      << static_cast<int>(Game::MAX_ITEM_OPTIONS) << "개 초과" << std::endl;
            ok = false;
        }
    }
    return ok;
}

const ItemData* ItemTemplate::GetData(uint32_t itemID) const
{
    auto it = data_.find(itemID);
    return it == data_.end() ? nullptr : &it->second;
}