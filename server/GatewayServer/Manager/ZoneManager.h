#pragma once
#include "pch.h"
#include "Network/SendBuffer.h"

class ZoneServerSession; // 게이트웨이가 내부 Zone 서버와 맺은 TCP 커넥션 세션
using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;

class ZoneManager {
public:
	static ZoneManager* GetInstance() {
		static ZoneManager instance;
		return &instance;
	}

	// 존, 채널 생성
	void RegisterZone(uint32_t zoneID, uint32_t channelID, ZoneServerSessionRef session);
	// 존, 채널 해제
	void UnregisterZone(uint32_t zoneID, uint32_t channelID);

	// 클라이언트 패킷에 sessionId 헤더를 감싸서 타겟 Zone으로 포워딩
	void SendToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionId, SendBufferRef clientPacketBuffer);

	// 클라이언트 연결 종료를 담당 Zone에 통보
	void SendDisconnectToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionId);

private:
	ZoneManager() = default;
	~ZoneManager() = default;

	static uint64_t MakeKey(uint32_t zoneID, uint32_t channelId) {
		// 64비트중 32비트는zoneID 뒤에 32비트는 ChannelID로 나뉘어 관리
		return (static_cast<uint64_t>(zoneID) << 32) | channelId;
	}

private:
	std::mutex lock_;
	std::unordered_map<uint32_t, ZoneServerSessionRef> zoneServers_;
};