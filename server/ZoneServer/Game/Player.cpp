#include "pch.h"
#include "Player.h"


// ... 기존 생성자에 z_(0) 추가
Player::Player(uint64_t playerID, std::string name, float x, float y, float z, int hp, int mp, uint64_t exp)
	: playerID_(playerID), name_(name), x_(x), y_(y), z_(z), hp_(hp), mp_(mp), exp_(exp) {

}

void Player::SetPosition(float x, float y, float z) {
	x_ = x; y_ = y; z_ = z;
}

bool Player::TryMove(float x, float y, float z, float maxSpeed)
{
	// Nan / infinite 거부
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))return false;

	auto now = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(now - lastMoveTime_).count();
	lastMoveTime_ = now;
	// 지난 호출 이후 흐른 시간만큼 최대 이동 가능 거리 계산
	moveBudget_ = std::min(moveBudget_ + elapsed * maxSpeed, maxSpeed * 1.0f);

	float dx = x - x_, dy = y - y_, dz = z - z_;
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	// 최대 이동 가능 거리 초과
	if (dist > moveBudget_) return false;

	moveBudget_ -= dist;
	SetPosition(x, y, z);
	return true;
}

bool Player::CheckSKillCost(SkillCostType type, int32_t cost) const
{
	switch (type)
	{
	case SkillCostType::None: return true;
	case SkillCostType::Hp: return hp_ > cost;
	case SkillCostType::Mp: return mp_ >= cost;
	default:	return false;
	}
}

void Player::SpendSkillCost(SkillCostType type, int32_t cost)
{
	switch (type) {
	case SkillCostType::Hp: hp_ -= cost;
		break;

	case SkillCostType::Mp:	mp_ -= cost;
		break;
	}
}

bool Player::TrySpendSkill(SkillCostType type, int32_t cost)
{
	// 차감 가능 여부 체크
	if (!CheckSKillCost(type, cost)) return false;

	// 차감 처리
	SpendSkillCost(type, cost);

	return true;
}


