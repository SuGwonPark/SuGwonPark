#include "pch.h"
#include "Player.h"
#include "Manager/SendBufferManager.h"
#include "Network/GatewaySession.h"

// ... 기존 생성자에 z_(0) 추가
Player::Player(int id, std::string name)
	: id_(id), name_(name), x_(0), y_(0), z_(0), hp_(100), exp_(0)
	, playerId_(static_cast<uint64_t>(id)) {
}

void Player::SetPosition(float x, float y, float z) {
	x_ = x; y_ = y; z_ = z;
}


void Player::Send(const void* data, uint32_t length) {
	if (sessionID_ == 0) return;   // 아직 로그인 세션 안 붙은 플레이어는 무시

	uint16_t totalSize = static_cast<uint16_t>(sizeof(InternalPacketHeader) + length);
	SendBufferRef sendBuffer = SendBufferManager::Open(totalSize);

	InternalPacketHeader header{};
	header.size = totalSize;
	header.id = reinterpret_cast<const PacketHeader*>(data)->id;  // 실제 게임 패킷의 id 유지
	header.sessionID = sessionID_;

	sendBuffer->Write(&header, sizeof(header));
	sendBuffer->Write(data, length);
	SendBufferManager::Close(totalSize);

	GatewaySession::GetInstance()->Send(sendBuffer);   // Gateway로 전송 -> HandleInternalPacket이 relay
}