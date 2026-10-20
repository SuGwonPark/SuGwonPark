#include "pch.h"
#include "Monster.h"

Monster::Monster(uint64_t monsterUID, const MonsterData* data, float x, float y, float z)
	: monsterUID_(monsterUID), data_(data)
	, x_(x), y_(y), z_(z)
	, spawnX_(x), spawnY_(y), spawnZ_(z)
	, hp_(data_->maxHp) {
}
