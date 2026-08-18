#pragma once
#include "pch.h"
#include "Network/Protocol.h"
#include "Network/SendBuffer.h"

class ZoneServerSession : public std::enable_shared_from_this<ZoneServerSession> {
	enum { RECV_BUFFER_SIZE = 65536 * 2 }; // 내부 백본용 대용량 버퍼 (128KB)

public:
	ZoneServerSession(uint32_t zoneId, net::io_context& ioc);
	~ZoneServerSession();

	void Start();
	void Close();

	// Gateway -> ZoneServer 패킷 전송 (ZoneManager가 호출)
	void Send(SendBufferRef sendBuffer);

	tcp::socket& Socket() { return socket_; }
	uint32_t GetZoneId() const { return zoneId_; }

private:
	void DoRead();
	void OnRead(const boost::system::error_code& ec, size_t bytesTransferred);
	void DoWrite();
	void OnWrite(const boost::system::error_code& ec, size_t bytesTransferred);

	void ProcessPackets();
	void HandleInternalPacket(uint8_t* buffer, uint16_t size);

private:
	const uint32_t zoneId_;
	tcp::socket socket_;
	net::strand<net::io_context::executor_type> strand_;

	std::atomic<bool> isConnected_{ false };

	// 수신 스트림 버퍼
	std::array<uint8_t, RECV_BUFFER_SIZE> recvBuffer_;
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	// 송신 큐
	std::queue<SendBufferRef> sendQueue_;
};

using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;