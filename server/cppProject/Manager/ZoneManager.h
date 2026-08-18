#pragma once
#include "pch.h"
#include "SendBuffer.h"

// 게이트웨이 <-> 내부 Zone 서버 간 패킷 래퍼 헤더
#pragma pack(push, 1)
struct InternalPacketHeader {
	uint16_t size;        // 내부 패킷 전체 크기
	uint16_t id;          // 내부 패킷 ID
	uint64_t sessionId;   // 클라이언트 세션 고유 ID
};
#pragma pack(pop)

class ZoneServerSession; // 게이트웨이가 내부 Zone 서버와 맺은 TCP 커넥션 세션
using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;

class ZoneManager {
public:
	static ZoneManager* GetInstance() {
		static ZoneManager instance;
		return &instance;
	}

	void RegisterZone(uint32_t zoneId, ZoneServerSessionRef session);
	void UnregisterZone(uint32_t zoneId);

	// 클라이언트 패킷에 sessionId 헤더를 감싸서 타겟 Zone으로 포워딩
	void SendToZone(uint32_t zoneId, uint64_t sessionId, SendBufferRef clientPacketBuffer);

	// 클라이언트 연결 종료를 담당 Zone에 통보
	void SendDisconnectToZone(uint32_t zoneId, uint64_t sessionId);

private:
	ZoneManager() = default;
	~ZoneManager() = default;

private:
	std::mutex lock_;
	std::unordered_map<uint32_t, ZoneServerSessionRef> zoneServers_;
};