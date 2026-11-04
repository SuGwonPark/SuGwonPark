#include "pch.h"
#include "Template/SkillTemplate.h"
#include "Template/SkillEffectTemplate.h"
#include "ServerCore/Utils/CsvReader.h"
#include "ServerCore/Utils/util.h" 

bool SkillTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Skill.csv", [this](const CsvRowView& r) {
		SkillData d{};
		d.skillID = r.U32("ID");
		d.classType = ParseCsvEnum2<ClassType>(r.Str("ClassType"), "ClassType", d.skillID); // None = 공용 스킬
		d.targetType = ParseCsvEnum<TargetType>(r.Str("TargetType"), "TargetType", d.skillID);
		d.rangeType = ParseCsvEnum<RangeType>(r.Str("RangeType"), "RangeType", d.skillID);
		d.range = r.Float("Range");
		d.radius = r.Float("Radius");
		d.cooldownMs = r.I32("CooldownMs");
		d.castingMs = r.I32("CastingMs");
		d.costType = ParseCsvEnum2<SkillCostType>(r.Str("CostType"), "CostType", d.skillID);
		d.cost = r.I32("Cost");
		d.effectGroupID = r.U32("EffectGroupID");

		if (d.cooldownMs < 0)   throw std::invalid_argument("CooldownMs는 0 이상");
		if (d.castingMs < 0)    throw std::invalid_argument("CastingMs는 0 이상");
		if (d.cost < 0)         throw std::invalid_argument("Cost는 0 이상");
		if (d.costType == SkillCostType::None && d.cost != 0)
			throw std::invalid_argument("CostType이 None이면 Cost는 0이어야 함");
		if (d.effectGroupID == 0) throw std::invalid_argument("EffectGroupID는 1 이상");

		// 본인 대상(Self) 스킬은 사거리가 필요 없다
		if (d.rangeType == RangeType::Single && d.targetType != TargetType::Self && d.range <= 0.f)
			throw std::invalid_argument("Single 스킬은 Range가 0보다 커야 함");
		if (d.rangeType == RangeType::SelfArea && d.radius <= 0.f)
			throw std::invalid_argument("SelfArea 스킬은 Radius가 0보다 커야 함");
		if (d.rangeType == RangeType::TargetArea && (d.range <= 0.f || d.radius <= 0.f))
			throw std::invalid_argument("TargetArea 스킬은 Range와 Radius가 0보다 커야 함");

		if (!data_.emplace(Key(d.classType, d.skillID), d).second)
			throw std::invalid_argument("중복 (ClassType, ID): " + r.Str("ClassType") + ", " + r.Str("ID"));
		});
}

bool SkillTemplate::Validate() const
{
	bool ok = true;
	const SkillEffectTemplate& effects = SkillEffectTemplate::Instance();

	for (const auto& [key, s] : data_) {
		const std::vector<SkillEffectData>* group = effects.GetGroup(s.effectGroupID);
		if (!group) {
			std::cerr << "[Template] Skill.csv ID " << s.skillID << ": SkillEffect.csv에 없는 EffectGroupID "
				<< s.effectGroupID << std::endl;
			ok = false;
			continue;
		}
	}
	return ok;
}

const SkillData* SkillTemplate::GetData(ClassType classType, uint32_t skillID) const
{
	auto it = data_.find(Key(classType, skillID));
	if (it != data_.end()) return &it->second;

	// 직업 전용에 없으면 공용 스킬(ClassType::None)에서 찾는다
	if (classType != ClassType::None) {
		it = data_.find(Key(ClassType::None, skillID));
		if (it != data_.end()) return &it->second;
	}
	return nullptr;
}
