#include "pch.h"
#include "Template/StatTemplate.h"
#include "Template/LevelTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h" // ParseCsvEnum, TypeToString, magic_enum

bool StatTemplate::Load()
{
    data_.clear();
    return Csv::ForEachRow("Stat.csv", [this](const CsvRow& r) {
        StatData d{};
        d.classType = ParseCsvEnum<ClassType>(r.at(1), "ClassType");
        const uint32_t level = Csv::ToU32(r.at(2));
        d.maxHp = Csv::ToI32(r.at(3));
        d.maxMp = Csv::ToI32(r.at(4));
        d.damage = Csv::ToI32(r.at(5));

        if (level == 0 || level > 0xFFFF)
            throw std::invalid_argument("Level은 1 ~ 65535");
        d.level = static_cast<uint16_t>(level);
        if (d.maxHp <= 0)
            throw std::invalid_argument("Hp는 1 이상");
        if (d.maxMp < 0)
            throw std::invalid_argument("Mp는 0 이상");
        if (d.damage <= 0)
            throw std::invalid_argument("Damage는 1 이상");

        if (!data_.emplace(Key(d.classType, d.level), d).second)
            throw std::invalid_argument("중복 (ClassType, Level): " + r.at(1) + ", " + r.at(2));
    });
}

bool StatTemplate::Validate() const
{
    const uint16_t maxLevel = LevelTemplate::Instance().MaxLevel();
    if (maxLevel == 0)
    {
        std::cerr << "[Stat] Level.csv가 비어 있음" << std::endl;
        return false;
    }

    bool ok = true;
    for (ClassType c : magic_enum::enum_values<ClassType>())
    {
        if (c == ClassType::None)
            continue;
        for (uint16_t lv = 1; lv <= maxLevel; ++lv)
        {
            if (!Get(c, lv))
            {
                std::cerr << "[Stat] 누락: " << TypeToString(c) << " " << lv << "레벨" << std::endl;
                ok = false;
            }
        }
    }
    return ok;
}

const StatData* StatTemplate::Get(ClassType classType, uint16_t level) const
{
    auto it = data_.find(Key(classType, level));
    return it == data_.end() ? nullptr : &it->second;
}