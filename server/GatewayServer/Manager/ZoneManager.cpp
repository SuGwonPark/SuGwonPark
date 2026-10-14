#include "pch.h"
#include "Manager/ZoneManager.h"
#include "Manager/SendBufferManager.h"
#include "Network/ZoneServerSession.h"
#include "Protocol/InternalPackets.h"

// 존 생성
void ZoneManager::RegisterZone(uint32_t zoneID, uint32_t channelID, ZoneServerSessionRef session) {
	std::lock_guard<std::mutex> lock(lock_);
	zoneServers_[MakeZoneKey(zoneID, channelID)] = session;
}

void ZoneManager::UnregisterZone(uint32_t zoneID, uint32_t channelID, ZoneServerSessionRef session) {
	std::lock_guard<std::mutex> lock(lock_);
	auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));

	// 존 서버가 있으면서 세션이 같으면 제거
	if (it != zoneServers_.end() && it->second == session) {
		zoneServers_.erase(it);
	}
}

void ZoneManager::SendToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionID, SendBufferRef clientPacketBuffer) {
	ZoneServerSessionRef targetSession = nullptr;
	{
		std::lock_guard<std::mutex> lock(lock_);
		auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));
		if (it != zoneServers_.end()) {
			targetSession = it->second;
		}
	}

	// 존, 채널을 찾지 못하면 리턴
	if (!targetSession) return;

	InternalPacketHeader header{};
	header.header.id = PKT_INTERNAL_FORWARD;
	header.sessionID = sessionID;
	header.zoneID = zoneID;
	header.channelID = channelID;

	SendBufferRef wrapped = SendBufferManager::MakeWithPayload(
		header, clientPacketBuffer->Buffer(), clientPacketBuffer->AllocSize());
	if (!wrapped) return;

	targetSession->Send(wrapped);
}

void ZoneManager::SendDisconnectToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionID) {
	// 세션 종료 알림 패킷을 해당 Zone으로 전송하여 메모리 정리 유도
	InternalPacketHeader header{};
	header.header.size = sizeof(InternalPacketHeader);
	header.header.id = PKT_INTERNAL_DISCONNECT; // DISCONNECT 패킷 ID
	header.sessionID = sessionID;
	header.zoneID = zoneID;
	header.channelID = channelID;

	SendBufferRef sendBuffer = SendBufferManager::Make(&header, sizeof(header));

	std::lock_guard<std::mutex> lock(lock_);
	auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));
	if (it != zoneServers_.end()) {
		it->second->Send(sendBuffer);
	}
}