#pragma once
#include "pch.h"
#include "Network/Protocol.h"
#include "Network/SendBuffer.h"

class ClientProxySession : public std::enable_shared_from_this<ClientProxySession> {
	enum { RECV_BUFFER_SIZE = 65536 };

public:
	ClientProxySession(uint64_t sessionId, net::io_context& ioc);
	~ClientProxySession();

	void Start();
	void Close();

	// 클라이언트로 패킷 송신 (Zone/Chat 서버에서 온 응답을 클라이언트로 보낼 때)
	void Send(SendBufferRef sendBuffer);

	// Zone 1 완료 신호 수신 시 호출 -> 타겟 Zone 교체
	void OnZone1LeaveCompleted(uint32_t nextZoneId);

	// Getters & Setters
	tcp::socket& Socket() { return socket_; }
	uint64_t GetSessionId() const { return sessionId_; }
	uint32_t GetCurrentZoneId() const { return currentZoneId_.load(std::memory_order_relaxed); }
	void SetCurrentZoneId(uint32_t zoneId) { currentZoneId_.store(zoneId, std::memory_order_relaxed); }

private:
	// 네트워크 I/O 루프
	void DoRead();
	void OnRead(const boost::system::error_code& ec, size_t bytesTransferred);
	void DoWrite();
	void OnWrite(const boost::system::error_code& ec, size_t bytesTransferred);

	// 수신 스트림 패킷 파싱 및 내부 라우팅
	void ProcessPackets();
	void RoutePacket(uint16_t packetId, uint8_t* packetPtr, uint16_t packetSize);

private:
	const uint64_t sessionId_;
	tcp::socket socket_;
	net::strand<net::io_context::executor_type> strand_;

	// 현재 패킷을 전달할 Zone 서버 ID (Drain 완료 시 원자적 교체)
	std::atomic<uint32_t> currentZoneId_{ 1 };
	std::atomic<bool> isConnected_{ false };

	// 수신 버퍼
	std::array<uint8_t, RECV_BUFFER_SIZE> recvBuffer_;
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	// 송신 큐 (Strand로 동기화)
	std::queue<SendBufferRef> sendQueue_;
};

using ClientProxySessionRef = std::shared_ptr<ClientProxySession>;