#include "pch.h"
#include "Template/MonsterTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool MonsterTemplate::Load()
{
    data_.clear();
    return Csv::ForEachRow("Monster.csv", [this](const CsvRowView& r) {
        MonsterData d{};
        d.monsterID = r.U32("ID");
        d.name = r.Str("Name");

        // 전투 스탯
        d.stat.maxHp = r.I32("MaxHp");
        d.stat.damage = r.I32("Attack");
        d.stat.defense = r.I32("Defense");
        d.stat.critRate = r.I32("CritRate");
        d.stat.critDamage = r.I32("CritDamage");
        d.stat.moveSpeed = r.Float("MoveSpeed");

        // 행동 수치
        d.ai.aggroRange = r.Float("AggroRange");
        d.ai.attackRange = r.Float("AttackRange");
        d.ai.leashRange = r.Float("LeashRange");
        d.ai.attackCooldownMs = r.I32("AttackCooldownMs");

        d.rewardGroupID = r.U32("RewardID");
        d.respawnSec = r.I32("ResponSec");

        const int32_t rateMax = static_cast<int32_t>(BuffMaxPercent);
        if (d.name.empty())
            throw std::invalid_argument("Name이 비어 있음");
        if (d.stat.maxHp <= 0)
            throw std::invalid_argument("MaxHp는 1 이상");
        if (d.stat.damage < 0)
            throw std::invalid_argument("Attack은 0 이상");
        if (d.stat.defense < 0)
            throw std::invalid_argument("Defense는 0 이상");
        if (d.stat.critRate < 0 || d.stat.critRate > rateMax)
            throw std::invalid_argument("CritRate는 0 ~ " + std::to_string(rateMax));
        if (d.stat.critDamage < rateMax)
            throw std::invalid_argument("CritDamage는 " + std::to_string(rateMax) + "(100%) 이상");
        if (d.stat.moveSpeed < 0.f)
            throw std::invalid_argument("MoveSpeed는 0 이상");
        if (d.ai.attackRange <= 0.f)
            throw std::invalid_argument("AttackRange는 0보다 커야 함");
        if (d.ai.aggroRange < d.ai.attackRange)
            throw std::invalid_argument("AggroRange는 AttackRange 이상");
        if (d.ai.leashRange < d.ai.aggroRange)
            throw std::invalid_argument("LeashRange는 AggroRange 이상");
        if (d.ai.attackCooldownMs <= 0)
            throw std::invalid_argument("AttackCooldownMs는 1 이상");
        if (d.respawnSec < 0)
            throw std::invalid_argument("ResponSec는 0 이상");
        if (!data_.emplace(d.monsterID, d).second)
            throw std::invalid_argument("중복 ID " + r.Str("ID"));
    });
}
const MonsterData* MonsterTemplate::GetData(uint32_t monsterID) const
{
	auto it = data_.find(monsterID);
	return it == data_.end() ? nullptr : &it->second;
}
