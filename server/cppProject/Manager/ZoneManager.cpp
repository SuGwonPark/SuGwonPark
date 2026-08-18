#include "pch.h"
#include "ZoneManager.h"
#include "SendBufferManager.h"
#include "ZoneServerSession.h"

void ZoneManager::RegisterZone(uint32_t zoneId, ZoneServerSessionRef session) {
	std::lock_guard<std::mutex> lock(lock_);
	zoneServers_[zoneId] = session;
}

void ZoneManager::UnregisterZone(uint32_t zoneId) {
	std::lock_guard<std::mutex> lock(lock_);
	zoneServers_.erase(zoneId);
}

void ZoneManager::SendToZone(uint32_t zoneId, uint64_t sessionId, SendBufferRef clientPacketBuffer) {
	ZoneServerSessionRef targetSession = nullptr;
	{
		std::lock_guard<std::mutex> lock(lock_);
		auto it = zoneServers_.find(zoneId);
		if (it != zoneServers_.end()) {
			targetSession = it->second;
		}
	}

	if (!targetSession) return;

	// [패킷 래핑] 기존 패킷 앞에 12바이트 InternalPacketHeader를 덧붙여 전송
	uint16_t totalSize = sizeof(InternalPacketHeader) + clientPacketBuffer->AllocSize();
	SendBufferRef wrappedBuffer = SendBufferManager::Open(totalSize);

	InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(wrappedBuffer->Buffer());
	header->size = totalSize;
	header->id = 9999; // 내부 포워딩용 패킷 ID
	header->sessionId = sessionId;

	// 뒤이어 원본 클라이언트 패킷 바이너리 복사
	std::memcpy(wrappedBuffer->Buffer() + sizeof(InternalPacketHeader),
		clientPacketBuffer->Buffer(),
		clientPacketBuffer->AllocSize());

	SendBufferManager::Close(totalSize);
	targetSession->Send(wrappedBuffer);
}

void ZoneManager::SendDisconnectToZone(uint32_t zoneId, uint64_t sessionId) {
	// 세션 종료 알림 패킷을 해당 Zone으로 전송하여 메모리 정리 유도
	InternalPacketHeader header;
	header.size = sizeof(InternalPacketHeader);
	header.id = 9998; // DISCONNECT 패킷 ID
	header.sessionId = sessionId;

	SendBufferRef sendBuffer = SendBufferManager::Open(sizeof(header));
	sendBuffer->Write(&header, sizeof(header));
	SendBufferManager::Close(sizeof(header));

	std::lock_guard<std::mutex> lock(lock_);
	auto it = zoneServers_.find(zoneId);
	if (it != zoneServers_.end()) {
		it->second->Send(sendBuffer);
	}
}