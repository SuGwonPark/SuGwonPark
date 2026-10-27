#pragma once
#include "GatewayConstants.h"
#include "pch.h"
#include "ServerCore/Protocol/Packets.h"
#include "ServerCore/Network/SendBuffer.h"
#include "ServerCore/Utils/util.h"

// 세션 상태값
enum class SessionState : uint8_t
{
    Connected,
    LoggingIn,
    Lobby,
    Creating, 
    Selecting,
    InGame,
};

class ClientProxySession : public std::enable_shared_from_this<ClientProxySession> {


public:
	ClientProxySession(uint64_t sessionID, net::io_context& ioc);
	~ClientProxySession();

	void Start();
	void Close();

	// 클라이언트로 패킷 송신 (Zone 서버에서 온 응답을 클라이언트로 보낼 때)
	void Send(SendBufferRef sendBuffer);

	// Zone 1 완료 신호 수신 시 호출 -> 타겟 Zone 교체
	void OnZone1LeaveCompleted(uint32_t nextZoneID);

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

	SessionState GetState() const { return state_.load(); }
	void SetState(SessionState state) { return state_.store(state); }

	// 현재 상태가 일치할때만 to로 교체
	bool TryTransition(SessionState from, SessionState to) {
		return state_.compare_exchange_strong(from, to);
	}
		
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
	std::atomic<SessionState> state_{ SessionState::Connected };

	// 수신 버퍼
	std::array<uint8_t, GatewayConst::CLIENT_RECV_BUFFER_SIZE> recvBuffer_;
	size_t readPos_ = 0;
	size_t writePos_ = 0;

	// 송신 큐 (Strand로 동기화)
	std::queue<SendBufferRef> sendQueue_;

	// 이만큼 못 보내고 쌓이면 읽지 않는(또는 너무 느린) 클라이언트로 보고 끊는다
	static constexpr size_t MAX_SEND_QUEUE_BYTES = 1 * 1024 * 1024;   // 1MB
	size_t sendQueueBytes_ = 0;      // strand 안에서만 접근


};

using ClientProxySessionRef = std::shared_ptr<ClientProxySession>;