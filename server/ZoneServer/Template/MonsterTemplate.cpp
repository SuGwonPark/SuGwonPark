#include "pch.h"
#include "Template/MonsterTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool MonsterTemplate::Load()
{
	data_.clear();
	return Csv::ForEachRow("Monster.csv", 9, [this](const CsvRow& r) {
		MonsterData d{};
		d.monsterID = Csv::ToU32(r[0]);
		d.name = r[1];
		d.maxHp = Csv::ToI32(r[2]);
		d.attack = Csv::ToI32(r[3]);
		d.moveSpeed = Csv::ToFloat(r[4]);
		d.aggroRange = Csv::ToFloat(r[5]);
		d.attackRange = Csv::ToFloat(r[6]);
		d.rewardID = Csv::ToU32(r[7]);
		d.respawnSec = Csv::ToI32(r[8]);

		if (d.name.empty())                  throw std::invalid_argument("Name이 비어 있음");
		if (d.maxHp <= 0)                    throw std::invalid_argument("Hp는 1 이상");
		if (d.attack < 0)                    throw std::invalid_argument("Attack은 0 이상");
		if (d.moveSpeed < 0.f)               throw std::invalid_argument("MoveSpeed는 0 이상");
		if (d.attackRange <= 0.f)            throw std::invalid_argument("AttackRange는 0보다 커야 함");
		if (d.aggroRange < d.attackRange)    throw std::invalid_argument("AggroRange는 AttackRange 이상");
		if (d.respawnSec < 0)                throw std::invalid_argument("ResponSec는 0 이상");
		if (!data_.emplace(d.monsterID, d).second) throw std::invalid_argument("중복 ID " + r[0]);
		});
}
const MonsterData* MonsterTemplate::GetData(uint32_t monsterID) const
{
	auto it = data_.find(monsterID);
	return it == data_.end() ? nullptr : &it->second;
}
