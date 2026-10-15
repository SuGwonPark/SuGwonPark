#include "pch.h"
#include "Player.h"

// ... 기존 생성자에 z_(0) 추가
Player::Player(uint64_t playerID, std::string name, float x, float y, float z, int hp, int mp, uint64_t exp)
	: playerID_(playerID), name_(name), x_(x), y_(y), z_(z), hp_(hp), mp_(mp), exp_(exp) {

}

void Player::SetPosition(float x, float y, float z) {
	x_ = x; y_ = y; z_ = z;
}

