#pragma once
#include "pch.h"
#include "GatewayConstants.h"
#include "ServerCore/Network/SendBuffer.h"
#include "ServerCore/Protocol/InternalPackets.h"

class ZoneServerSession : public std::enable_shared_from_this<ZoneServerSession> {
public:
	ZoneServerSession(net::io_context& ioc);
	~ZoneServerSession();

	void Start();
	void Close();

	// Gateway -> ZoneServer 패킷 전송 (ZoneManager가 호출)
	void Send(SendBufferRef sendBuffer);

	tcp::socket& Socket() { return socket_; }

private:
	void DoRead();
	void DoWrite();

	void ProcessPackets();
	// 유저에게 전달
	void HandleInternalPacket(uint8_t* buffer, uint16_t size);

private:

	std::vector<ZoneKeyEntry> zoneKeys_;

	bool handshakeDone_ = false;  // 추가

	tcp::socket socket_;
	net::strand<net::io_context::executor_type> strand_;

	std::atomic<bool> isConnected_{ false };

	// 수신 스트림 버퍼
	std::array<uint8_t, GatewayConst::ZONE_RECV_BUFFER_SIZE> recvBuffer_;
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	// 송신 큐
	std::queue<SendBufferRef> sendQueue_;
};

using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;