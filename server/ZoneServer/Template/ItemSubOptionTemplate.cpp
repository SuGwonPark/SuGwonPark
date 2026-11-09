#include "pch.h"
#include "Template/ItemSubOptionTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"

bool ItemSubOptionTemplate::Load()
{
    groups_.clear();
    std::unordered_set<uint32_t> ids;

    return Csv::ForEachRow("ItemSubOption.csv", [&](const CsvRowView& r) {
        ItemSubOptionData d{};
        d.id = r.U32("ID");
        d.groupID = r.U32("GroupID");
        d.statType = ParseCsvEnum<StatType>(r.Str("StatType"), "StatType", d.id);
        d.valueType = ParseCsvEnum<ValueType>(r.Str("ValueType"), "ValueType", d.id);
        d.minValue = r.I32("MinValue");
        d.maxValue = r.I32("MaxValue");
        d.rate = r.U32("Rate");

        if (d.groupID == 0)
            throw std::invalid_argument("GroupID는 1 이상");
        if (d.minValue <= 0)
            throw std::invalid_argument("MinValue는 1 이상");
        if (d.minValue > d.maxValue)
            throw std::invalid_argument("MinValue가 MaxValue보다 큼");
        if (d.rate == 0 || d.rate > RewardMaxRate)
            throw std::invalid_argument("Rate는 1 ~ " + std::to_string(RewardMaxRate));
        if (!ids.insert(d.id).second)
            throw std::invalid_argument("중복 ID " + r.Str("ID"));

        Group& g = groups_[d.groupID];
        g.rows.push_back(d);
        g.weights.push_back(static_cast<int>(d.rate));
    });
}

void ItemSubOptionTemplate::Roll(uint32_t groupID, uint32_t count, std::vector<ItemOptionInfo>& out) const
{
    auto it = groups_.find(groupID);
    if (it == groups_.end())
        return;

    // 뽑힌 줄은 가중치를 0으로 → 다음 뽑기에서 제외 (WeightedIndex가 0은 건너뜀)
    std::vector<int> weights = it->second.weights;
    for (uint32_t i = 0; i < count; ++i)
    {
        const int index = Random::WeightedIndex(weights);
        if (index < 0)
            break; // 다 뽑음

        const ItemSubOptionData& d = it->second.rows[index];
        out.push_back({d.statType, d.valueType, Random::Int(d.minValue, d.maxValue)});
        weights[index] = 0;
    }
}