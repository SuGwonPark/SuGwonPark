#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "ServerCore/Protocol/VarEnum.h"   // SkillTargetType

// Data/Skill.csv : ID,Name,TargetType,Damage,Range,Radius,CooldownMs,MpCost
struct SkillData {
	uint32_t        skillID;
	std::string     name;
	SkillTargetType targetType;
	int32_t         damage;
	float           range;        // Single: 대상까지 최대 거리
	float           radius;       // SelfArea: 시전자 중심 반경
	int32_t         cooldownMs;
	SkillCostType	costType;
	int32_t         cost;
};

class SkillTemplate {
public:
	static SkillTemplate& Instance() { static SkillTemplate inst; return inst; }

	bool Load();
	const SkillData* GetData(uint32_t skillID) const;   // 없으면 nullptr
	const std::unordered_map<uint32_t, SkillData>& All() const { return data_; }

private:
	std::unordered_map<uint32_t, SkillData> data_;
};