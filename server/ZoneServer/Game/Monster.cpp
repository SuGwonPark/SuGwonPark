#include "pch.h"
#include "Monster.h"

Monster::Monster(uint64_t monsterUID, const MonsterData* data, float x, float y, float z)
	: monsterUID_(monsterUID), data_(data)
	, x_(x), y_(y), z_(z)
	, spawnX_(x), spawnY_(y), spawnZ_(z)
	, maxHp_(data_->maxHp), hp_(data_->maxHp) {
}

// 제곱 사용 (대상 범위도 제곱해야함)
float Monster::DistSqTo(float x, float y, float z) const
{
	float dx = x - x_, dy = y - y_, dz = z - z_;
	return dx * dx + dy * dy + dz * dz;
}

// 타겟 남은 거리 체크
bool Monster::MoveToward(float tx, float ty, float tz, float step, float stopDist)
{
	float dx = tx - x_, dy = ty - y_, dz = tz - z_;
	// 몬스터 위치에서 내 위치까지의 거리
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	// 남은 거리
	float remain = dist - stopDist;
	if (remain <= 0.f || step <= 0.f) return false;

	// 목표물 앞 까지만 이동
	float move = std::min(step, remain);
	float ratio = move / dist;
	x_ += dx * ratio;
	y_ += dy * ratio;
	z_ += dz * ratio;

	return true;
}

int32_t Monster::TakeDamage(int32_t damage)
{
	if (IsDead() || damage <= 0) return 0;
	const int32_t applied = std::min(damage, hp_);
	hp_ -= applied;
	return applied;
}

void Monster::Die(Clock::time_point now)
{
	hp_ = 0;
	state_ = MonsterState::Dead;
	targetPlayerID_ = 0;
	// 리스폰 시간 세팅 (현재시간 + 리스폰 시간 이후 생성)
	respawnAt_ = now + std::chrono::seconds(data_->respawnSec);
}

// 초기화
void Monster::Respawn()
{
	x_ = spawnX_; y_ = spawnY_; z_ = spawnZ_;
	hp_ = data_->maxHp;
	SetIdle();
}

bool Monster::ResetHp()
{
	if (hp_ == data_->maxHp) return false;
	hp_ = data_->maxHp;
	return true;
}

