#pragma once
#include "RecvBuffer.h"
#include "SendBuffer.h"


using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
	explicit Session(tcp::socket socket);
	virtual ~Session();

	// 네트워크 I/O 시작
	void Start();

	// 외부(게임/로직 스레드)에서 호출하는 패킷 전송 (Lock-Free)
	void Send(SendBufferRef sendBuffer);

	// 세션 강제 종료
	void Disconnect(const std::string& reason = "");

	bool IsConnected() const { return isConnected_.load(); }
	tcp::socket& GetSocket() { return socket_; }

protected:
	void DoRead();
	void RegisterSend(SendBufferRef sendBuffer);
	void DoWrite();

	// TCP 스트림 파싱
	int32_t ProcessPacketStream(bool& outError);

	// [상속용 인터페이스]
	virtual void OnConnected() {}
	virtual void OnRecvPacket(uint8_t* packetBuffer, int32_t size) = 0;
	virtual void OnDisconnected() {}

private:
	tcp::socket socket_;
	// ★ Fix 1: 멀티스레드 경합을 완벽히 제거하는 Strand
	boost::asio::strand<boost::asio::any_io_executor> strand_;

	RecvBuffer recvBuffer_;

	// Strand 내부에서만 접근하므로 별도의 Mutex가 필요 없음 (성능 최적화)
	std::queue<SendBufferRef> sendQueue_;
	std::vector<SendBufferRef> pendingSendVector_;
	bool isWriting_ = false;

	// ★ Fix 2: Disconnect 중복 호출을 막는 원자적 플래그
	std::atomic<bool> isConnected_{ false };
};

using SessionRef = std::shared_ptr<Session>;