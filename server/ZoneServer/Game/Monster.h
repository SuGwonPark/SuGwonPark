#pragma once
#include "Template/MonsterTemplate.h"

class Monster {
public:
	Monster(uint64_t monsterUID, const MonsterData* data, float x, float y, float z);

	uint64_t GetMonsterUID() const { return monsterUID_; }
	const MonsterData& getData() const { return *data_; }
	uint32_t GetTemplateID() const { return data_->monsterID; }

	float GetX() const { return x_; }
	float GetY() const { return y_; }
	float GetZ() const { return z_; }

	float GetSpawnX() const { return spawnX_; }
	float GetSpawnY() const { return spawnY_; }
	float GetSpawnZ() const { return spawnZ_; }

	int32_t GetHp() const { return hp_; }
	int32_t GetMaxHp() const { return data_->maxHp; }
	bool IsDead() const { return hp_ <= 0; }

private:
	uint64_t monsterUID_;
	const MonsterData* data_;    // 정적 테이블을 가리킴 (수명 걱정 없음)

	float x_, y_, z_;
	float spawnX_, spawnY_, spawnZ_;
	int32_t hp_;
};

using MonsterRef = std::shared_ptr<Monster>;