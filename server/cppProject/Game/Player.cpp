#include "pch.h"
#include "Player.h"
#include "Manager/SendBufferManager.h"

// ... 기존 생성자에 z_(0) 추가
Player::Player(int id, std::string name)
	: id_(id), name_(name), x_(0), y_(0), z_(0), hp_(100), exp_(0)
	, playerId_(static_cast<uint64_t>(id)) {
}

void Player::SetPosition(float x, float y, float z) {
	x_ = x; y_ = y; z_ = z;
}

void Player::Send(const void* data, uint32_t length) {
	if (!session_) return;   // 아직 로그인 세션이 연결 안 된 플레이어(봇/DB전용 등)는 조용히 무시

	SendBufferRef sendBuffer = SendBufferManager::Open(length);
	sendBuffer->Write(data, length);
	SendBufferManager::Close(length);

	session_->Send(sendBuffer);
}