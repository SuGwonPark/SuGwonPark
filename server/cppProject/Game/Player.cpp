#include "pch.h"
#include "Player.h"

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