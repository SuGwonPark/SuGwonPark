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
		d.maxHp = r.I32("MaxHp");
		d.attack = r.I32("Attack");
		d.moveSpeed = r.Float("MoveSpeed");
		d.aggroRange = r.Float("AggroRange");
		d.attackRange = r.Float("AttackRange");
		d.rewardGroupID = r.U32("RewardID");
		d.respawnSec = r.I32("ResponSec");

		if (d.name.empty())                  throw std::invalid_argument("Name이 비어 있음");
		if (d.maxHp <= 0)                    throw std::invalid_argument("Hp는 1 이상");
		if (d.attack < 0)                    throw std::invalid_argument("Attack은 0 이상");
		if (d.moveSpeed < 0.f)               throw std::invalid_argument("MoveSpeed는 0 이상");
		if (d.attackRange <= 0.f)            throw std::invalid_argument("AttackRange는 0보다 커야 함");
		if (d.aggroRange < d.attackRange)    throw std::invalid_argument("AggroRange는 AttackRange 이상");
		if (d.respawnSec < 0)                throw std::invalid_argument("ResponSec는 0 이상");
		if (!data_.emplace(d.monsterID, d).second) throw std::invalid_argument("중복 ID " + r.Str("ID"));
		});
}
const MonsterData* MonsterTemplate::GetData(uint32_t monsterID) const
{
	auto it = data_.find(monsterID);
	return it == data_.end() ? nullptr : &it->second;
}
