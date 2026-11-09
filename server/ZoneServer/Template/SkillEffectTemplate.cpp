#include "pch.h"
#include "Template/SkillEffectTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"
#include <unordered_set>

bool SkillEffectTemplate::Load()
{
    byGroup_.clear();
    count_ = 0;

    std::unordered_set<uint32_t> ids;
    return Csv::ForEachRow("SkillEffect.csv", [&](const CsvRowView& r) {
        SkillEffectData d{};
        d.groupID = r.U32("GroupID");
        d.effectType = ParseCsvEnum<SkillEffectType>(r.Str("EffectType"), "EffectType", r.U32("ID"));
        d.valueType = ParseCsvEnum<ValueType>(r.Str("ValueType"), "ValueType", r.U32("ID"));
        d.value = r.I32("Value");
        d.durationMs = r.I32("DurationMs");

        if (d.groupID == 0)
            throw std::invalid_argument("GroupID는 1 이상");
        if (d.value <= 0)
            throw std::invalid_argument("Value는 1 이상");
        if (d.durationMs < 0)
            throw std::invalid_argument("DurationMs는 0 이상");

        switch (d.effectType)
        {
            // 버프/ 디버프는 StatType으로 세팅
            case SkillEffectType::Buff:
            case SkillEffectType::Debuff:
                d.statType = ParseCsvEnum<StatType>(r.Str("EffectSubType"), "StatType", r.U32("ID"));
                if (d.statType == StatType::None)
                    throw std::invalid_argument("StatType이 필요");
                if (d.durationMs <= 0)
                    throw std::invalid_argument("DurationMs가 1 이상이어야 함");
                break;
                // 도트 스킬은 DotType으로 대체
            case SkillEffectType::DotDamage:
                d.dotType = ParseCsvEnum<DotType>(r.Str("EffectSubType"), "DotType", r.U32("ID"));
                if (d.dotType == DotType::None)
                    throw std::invalid_argument("DotDamage는 DotType이 필요");
                if (d.durationMs < ZoneConst::DOT_TICK_MS)
                    throw std::invalid_argument(
                        "DotDamage는 DurationMs가 " + std::to_string(ZoneConst::DOT_TICK_MS) + " 이상");
                break;

            default:
                if (r.Str("EffectSubType") != "None")
                    throw std::invalid_argument("즉시 효과는 EffectSubType이 None이어야 함");
                if (d.durationMs != 0)
                    throw std::invalid_argument("즉시 효과는 DurationMs가 0이어야 함");
                break;
        }

        // 회복/부활의 퍼센트는 만분율: 100%(10000)를 넘기지 않는다
        const bool heal = d.effectType == SkillEffectType::HealHp || d.effectType == SkillEffectType::HealMp ||
                          d.effectType == SkillEffectType::Revive;

        if (heal && d.valueType == ValueType::Percent && d.value > BuffMaxPercent)
            throw std::invalid_argument("회복/부활 Percent는 " + std::to_string(BuffMaxPercent) + "(100%) 이하");

        byGroup_[d.groupID].push_back(d);
        ++count_;
    });
}

const std::vector<SkillEffectData>* SkillEffectTemplate::GetGroup(uint32_t groupID) const
{
    auto it = byGroup_.find(groupID);
    return it == byGroup_.end() ? nullptr : &it->second;
}
