#pragma once
#include <cstdint>
#include "Protocol/Packets.h"   // PacketHeader

// Gateway <-> Zone 전용 패킷 번호 (클라이언트와 겹치지 않게 9000번대)
enum InternalPacketID : uint16_t {
	PKT_ZONE_HANDSHAKE = 9997,
	PKT_INTERNAL_DISCONNECT = 9998,
	PKT_INTERNAL_FORWARD = 9999,
};

#pragma pack(push, 1)

// 모든 내부 패킷 앞에 붙는 래퍼 헤더
struct InternalPacketHeader {
	PacketHeader header;
	uint64_t sessionID;
	uint32_t zoneID;
	uint32_t channelID;
};

// 핸드셰이크: 헤더 뒤에 ZoneKeyEntry가 count개 이어짐
struct ZoneKeyEntry {
	uint32_t zoneID;
	uint32_t channelID;
};

struct ZoneHandshakePacket {
	PacketHeader header;
	uint16_t count;
};

// Zone -> Gateway: 존 이동 준비 완료
struct RES_ZoneLeaveCompletedPacket {
	PacketHeader header;
	uint64_t playerID;
	uint32_t nextZoneID;
};

#pragma pack(pop)

inline uint64_t MakeZoneKey(uint32_t zoneID, uint32_t channelID) {
	return (static_cast<uint64_t>(zoneID) << 32) | channelID;
}