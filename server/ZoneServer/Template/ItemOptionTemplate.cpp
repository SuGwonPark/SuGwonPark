#include "pch.h"
#include "Template/ItemOptionTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"

bool ItemOptionTemplate::Load()
{
    groups_.clear();
    std::unordered_set<uint32_t> ids;

    return Csv::ForEachRow("ItemOption.csv", [&](const CsvRowView& r) {
        ItemOptionData d{};
        d.id = r.U32("ID");
        d.groupID = r.U32("GroupID");
        d.statType = ParseCsvEnum<StatType>(r.Str("StatType"), "StatType", d.id);
        d.valueType = ParseCsvEnum<ValueType>(r.Str("ValueType"), "ValueType", d.id);
        d.minValue = r.I32("MinValue");
        d.maxValue = r.I32("MaxValue");

        if (d.groupID == 0)
            throw std::invalid_argument("GroupID는 1 이상 (Item.csv에서 0은 '옵션 없음')");
        if (d.minValue <= 0)
            throw std::invalid_argument("MinValue는 1 이상");
        if (d.minValue > d.maxValue)
            throw std::invalid_argument("MinValue가 MaxValue보다 큼");
        if (!ids.insert(d.id).second)
            throw std::invalid_argument("중복 ID " + r.Str("ID"));

        groups_[d.groupID].push_back(d);
    });
}

void ItemOptionTemplate::GetGroup(uint32_t groupID, std::vector<ItemOptionInfo>& out) const
{
    auto it = groups_.find(groupID);
    if (it == groups_.end())
        return;

    for (const ItemOptionData& d : it->second)
        out.push_back({d.statType, d.valueType, Random::Int(d.minValue, d.maxValue)});
}