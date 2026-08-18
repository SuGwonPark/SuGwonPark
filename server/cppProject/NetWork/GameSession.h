#pragma once
#include "Network/Session.h"
#include "Network/Protocol.h"
#include "Enum/VarEnum.h"

class GameSession : public Session {
public:
	using Session::Session;

protected:
	void OnRecvPacket(uint8_t* packetBuffer, int32_t size) override {
		PacketHeader* header = reinterpret_cast<PacketHeader*>(packetBuffer);

		switch (header->id) {
		case PKT_C_MOVE:
			HandleMovePacket(packetBuffer, size);
			break;
		default:
			std::cout << "Unknown Packet ID: " << header->id << std::endl;
			break;
		}
	}

private:
	void HandleMovePacket(uint8_t* packetBuffer, int32_t size) {
		// 패킷 직렬화 해제 및 처리 로직 (예: Protobuf / FlatBuffers / Direct Cast)
		std::cout << "[GameSession] Processed Move Packet! Size: " << size << std::endl;
	}
};
