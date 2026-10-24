#include "pch.h"
#include "Template/SkillTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h"        // ParseCsvEnum<T>

bool SkillTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Skill.csv", [this](const CsvRow& r) {
		SkillData d{};
		d.skillID = Csv::ToU32(r.at(0));
		d.name = r.at(1);
		d.targetType = ParseCsvEnum<SkillTargetType>(r.at(2), "SkillTargetType");
		d.damage = Csv::ToI32(r.at(3));
		d.range = Csv::ToFloat(r.at(4));
		d.radius = Csv::ToFloat(r.at(5));
		d.cooldownMs = Csv::ToI32(r.at(6));
		d.costType = ParseCsvEnum2<SkillCostType>(r.at(7), "SkillCostType");
		d.cost = Csv::ToI32(r.at(8));

		if (d.name.empty())      throw std::invalid_argument("Name이 비어 있음");
		if (d.damage <= 0)       throw std::invalid_argument("Damage는 1 이상");
		if (d.cooldownMs < 0)    throw std::invalid_argument("CooldownMs는 0 이상");
		if (d.cost < 0)        throw std::invalid_argument("cost는 0 이상");
		if (d.targetType == SkillTargetType::Single && d.range <= 0.f)
			throw std::invalid_argument("Single 스킬은 Range가 0보다 커야 함");
		if (d.targetType == SkillTargetType::SelfArea && d.radius <= 0.f)
			throw std::invalid_argument("SelfArea 스킬은 Radius가 0보다 커야 함");
		if (d.targetType == SkillTargetType::TargetArea && (d.range <= 0.f || d.radius <= 0.f))
			throw std::invalid_argument("TargetArea 스킬은 Range와 Radius가 0보다 커야 함");
		if (!data_.emplace(d.skillID, d).second) throw std::invalid_argument("중복 ID " + r.at(0));
		});
}

const SkillData* SkillTemplate::GetData(uint32_t skillID) const
{
	auto it = data_.find(skillID);
	return it == data_.end() ? nullptr : &it->second;
}