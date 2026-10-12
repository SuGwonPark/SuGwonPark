#pragma once
#include "pch.h"
#include "Protocol/Packets.h"
#include "Network/SendBuffer.h"

class ClientProxySession : public std::enable_shared_from_this<ClientProxySession> {
	enum { RECV_BUFFER_SIZE = 65536 };

public:
	ClientProxySession(uint64_t sessionID, net::io_context& ioc);
	~ClientProxySession();

	void Start();
	void Close();

	// 클라이언트로 패킷 송신 (Zone/Chat 서버에서 온 응답을 클라이언트로 보낼 때)
	void Send(SendBufferRef sendBuffer);

	// Zone 1 완료 신호 수신 시 호출 -> 타겟 Zone 교체
	void OnZone1LeaveCompleted(uint32_t nextZoneId);

	// Getters & Setters
	tcp::socket& Socket() { return socket_; }
	uint64_t GetSessionID() const { return sessionID_; }
	uint32_t GetCurrentZoneID() const { return currentZoneID_.load(std::memory_order_relaxed); }
	void SetCurrentZoneID(uint32_t zoneId) { currentZoneID_.store(zoneId, std::memory_order_relaxed); }

	uint32_t GetCurrentChannelID() const { return currentChannelID_.load(std::memory_order_relaxed); }
	void SetCurrentChannelID(uint32_t channelID) { currentChannelID_.store(channelID, std::memory_order_relaxed); }

	uint64_t GetPlayerID() const { return playerID_.load(); }
	void SetPlayerID(uint64_t playerID) { playerID_.store(playerID); }

	uint64_t GetUserUID() const { return userUID_.load(); }
	void SetUserUID(uint64_t uid) { userUID_.store(uid); }

private:
	// 네트워크 I/O 루프
	void DoRead();
	void DoWrite();

	// 수신 스트림 패킷 파싱 및 내부 라우팅
	void ProcessPackets();
	void RoutePacket(uint16_t packetId, uint8_t* packetPtr, uint16_t packetSize);


private:
	// 세션 아이디
	const uint64_t sessionID_;
	// 유저 유니크 키
	std::atomic<uint64_t> userUID_{ 0 };
	// 캐릭터 유니크 키
	std::atomic<uint64_t> playerID_{ 0 };
	tcp::socket socket_;
	net::strand<net::io_context::executor_type> strand_;

	// 현재 패킷을 전달할 Zone 서버 ID (Drain 완료 시 원자적 교체)
	std::atomic<uint32_t> currentZoneID_{ 1 };
	std::atomic<uint32_t> currentChannelID_{ 1 };
	std::atomic<bool> isConnected_{ false };

	// 수신 버퍼
	std::array<uint8_t, RECV_BUFFER_SIZE> recvBuffer_;
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	// 송신 큐 (Strand로 동기화)
	std::queue<SendBufferRef> sendQueue_;

};

using ClientProxySessionRef = std::shared_ptr<ClientProxySession>;