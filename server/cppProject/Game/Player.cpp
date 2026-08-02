#include "pch.h"
#include "Player.h"
#include "Network/Session.h"

Player::Player(int id, std::string name)
	: id_(id), name_(name), x_(0), y_(0), hp_(100) {
}

void Player::Move(float x, float y) {
	x_ = x;
	y_ = y;
	std::cout << name_ << " 이동 → x: " << x_ << ", y: " << y_ << std::endl;
}

void Player::TakeDamage(int damage) {
	hp_ -= damage;
	if (hp_ < 0) hp_ = 0;
	std::cout << name_ << " 데미지: " << damage << " → 남은 HP: " << hp_ << std::endl;
}

bool Player::IsAlive() const {
	return hp_ > 0;
}

void Player::PrintStatus() const {
	std::cout << "[" << name_ << "] HP: " << hp_
		<< " 위치: (" << x_ << ", " << y_ << ")" << std::endl;
}

std::string Player::GetPlayerName()  const {
	return name_;
}

int Player::GetLevel() const {
	// 레벨업에 필요한 누적 경험치 테이블 (예시)
	std::vector<int> levelExpTable_ = std::vector<int>({ 0, 100, 500, 1500, 3000 });

	// std::upper_bound = 0(log N) 고속 탐색
	auto it = std::upper_bound(levelExpTable_.begin(), levelExpTable_.end(), exp_);
	// 레벨은 1부터 시작
	int level = static_cast<int>(std::distance(levelExpTable_.begin(), it)) + 1;

	int maxLevel = static_cast<int>(levelExpTable_.size()) + 1;
	return std::min(level, maxLevel); // 최대 레벨을 초과하지 않도록 제한

}
