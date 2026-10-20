#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

// Data/MonsterSpawn.csv : ID,ZoneID,MonsterID,Position
struct MonsterSpawnData {
	uint32_t spawnID;
	uint32_t zoneID;
	uint32_t monsterID;
	float    x, y, z;
};

class MonsterSpawnTemplate {
public:
	static MonsterSpawnTemplate& Instance() { static MonsterSpawnTemplate inst; return inst; }

	bool Load();
	bool Validate() const;

	const MonsterSpawnData* GetData(uint32_t spawnID) const;                // 없으면 nullptr
	const std::vector<MonsterSpawnData>& GetByZone(uint32_t zoneID) const;  // 없으면 빈 벡터
	const std::unordered_map<uint32_t, MonsterSpawnData>& All() const { return data_; }

private:
	std::unordered_map<uint32_t, MonsterSpawnData> data_;                   // spawnID -> 데이터
	std::unordered_map<uint32_t, std::vector<MonsterSpawnData>> byZone_;    // zoneID -> 그 존의 스폰 목록
};
