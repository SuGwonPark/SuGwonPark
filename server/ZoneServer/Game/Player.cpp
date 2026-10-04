#include "pch.h"
#include "Player.h"
#include "Manager/SendBufferManager.h"
#include "Network/GatewaySession.h"

// ... 기존 생성자에 z_(0) 추가
Player::Player(uint64_t playerID, std::string name, int x, int y, int z, int hp, uint64_t exp)
	: playerID_(playerID), name_(name), x_(x), y_(y), z_(z), hp_(hp), exp_(exp) {

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