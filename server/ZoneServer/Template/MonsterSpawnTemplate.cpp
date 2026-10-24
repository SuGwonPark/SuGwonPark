#include "pch.h"
#include "Template/MonsterSpawnTemplate.h"
#include "Template/MonsterTemplate.h"
#include "Template/ZoneTemplate.h"
#include "ServerCore/Utils/CsvReader.h"

bool MonsterSpawnTemplate::Load()
{
	data_.clear();
	byZone_.clear();
	return Csv::ForEachRow("MonsterSpawn.csv", 4, [this](const CsvRow& r) {
		MonsterSpawnData d{};
		d.spawnID   = Csv::ToU32(r[0]);
		d.zoneID    = Csv::ToU32(r[1]);
		d.monsterID = Csv::ToU32(r[2]);
		Csv::ToVec3(r[3], d.x, d.y, d.z);

		if (d.spawnID == 0)   throw std::invalid_argument("ID는 1 이상");
		if (d.zoneID == 0)    throw std::invalid_argument("ZoneID는 1 이상");
		if (d.monsterID == 0) throw std::invalid_argument("MonsterID는 1 이상");
		if (!data_.emplace(d.spawnID, d).second) throw std::invalid_argument("중복 ID " + r[0]);

		byZone_[d.zoneID].push_back(d);
		});
}

const MonsterSpawnData* MonsterSpawnTemplate::GetData(uint32_t spawnID) const
{
	auto it = data_.find(spawnID);
	return it == data_.end() ? nullptr : &it->second;
}

const std::vector<MonsterSpawnData>& MonsterSpawnTemplate::GetByZone(uint32_t zoneID) const
{
	static const std::vector<MonsterSpawnData> empty;
	auto it = byZone_.find(zoneID);
	return it == byZone_.end() ? empty : it->second;
}

// Zone.csv, Monster.csv 가 먼저 로드된 뒤에 호출
bool MonsterSpawnTemplate::Validate() const
{
	bool ok = true;
	for (const auto& [id, s] : data_) {
		if (!ZoneTemplate::Instance().GetData(s.zoneID)) {
			std::cerr << "[Template] MonsterSpawn.csv ID " << id << ": Zone.csv에 없는 ZoneID " << s.zoneID << std::endl;
			ok = false;
		}
		if (!MonsterTemplate::Instance().GetData(s.monsterID)) {
			std::cerr << "[Template] MonsterSpawn.csv ID " << id << ": Monster.csv에 없는 MonsterID " << s.monsterID << std::endl;
			ok = false;
		}
	}
	return ok;
}
