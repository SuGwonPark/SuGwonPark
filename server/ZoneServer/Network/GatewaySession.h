#pragma once
#include "pch.h"
#include "Network/Protocol.h"
#include "Network/SendBuffer.h"
#include "Game/Zone.h"

// Zone 서버 -> Gateway로 향하는 연결 (ZoneServerSession의 반대편 끝).
// Zone에서 Gateway로 보내야 하는 제어 패킷(예: Zone 이동 완료 ACK)을 여기로 보낸다.
// 프로세스 생애주기 내내 하나만 존재하는 싱글턴이라 shared_from_this는 쓰지 않는다.
class GatewaySession {
	enum { RECV_BUFFER_SIZE = 65536 };

public:
	static GatewaySession* GetInstance() {
		static GatewaySession inst;
		return &inst;
	}

	// Zone 프로세스 시작 시 한 번 호출해서 Gateway로 접속한다
	void Connect(net::io_context& ioc, const std::string& host, uint16_t port);

	// Zone -> Gateway 제어 패킷 전송 (strand로 직렬화됨)
	void Send(SendBufferRef sendBuffer);
	void AddZone(ZoneRef zone);

	void SendToClient(uint64_t sessionID, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size);
	bool IsConnected() const { return isConnected_.load(); }

private:
	GatewaySession() = default;

	void DoRead();
	void ProcessPackets();
	void HandleInternalPacket(uint8_t* buffer, uint16_t size);

	void DoWrite();

private:
	std::unordered_map<uint64_t, ZoneRef> zones_;
	uint32_t zoneID_ = 0;
	uint32_t channelID_ = 0;
	std::unique_ptr<tcp::socket> socket_;
	std::unique_ptr<net::strand<net::io_context::executor_type>> strand_;

	std::atomic<bool> isConnected_{ false };

	std::array<uint8_t, RECV_BUFFER_SIZE> recvBuffer_{};
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	std::queue<SendBufferRef> sendQueue_;
};