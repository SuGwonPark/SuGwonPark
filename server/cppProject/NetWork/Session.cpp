#include "pch.h"
#include "Network/Session.h"
#include "Network/Protocol.h"


Session::Session(tcp::socket socket)
	: socket_(std::move(socket)),
	// 💡 make_strand()를 사용하여 소켓의 executor로부터 Strand 생성
	strand_(boost::asio::make_strand(socket_.get_executor())),
	recvBuffer_(4096, 10) {
}

Session::~Session() {
	Disconnect("Destructor called");
}

void Session::Start() {
	isConnected_.store(true);
	OnConnected();
	DoRead();
}

void Session::Disconnect(const std::string& reason) {
	// ★ Fix 2 적용: CAS 연산으로 Disconnect 중복 실행 원천 차단
	if (isConnected_.exchange(false) == false) {
		return;
	}

	if (!reason.empty()) {
		std::cout << "[Session] Disconnected. Reason: " << reason << std::endl;
	}

	boost::system::error_code ec;
	socket_.shutdown(tcp::socket::shutdown_both, ec);
	socket_.close(ec);

	// 상위 게임 로직/세션 매니저에 연결 종료 알림 (단 1회만 호출됨)
	OnDisconnected();
}

void Session::DoRead() {
	if (!isConnected_.load()) return;

	// 1. 읽은 데이터를 버퍼 앞으로 정렬
	recvBuffer_.Clean();

	// ★ Fix 3 적용: 버퍼 풀(Full) 상태 체크 (무한 루프 및 CPU 100% 방어)
	if (recvBuffer_.FreeSize() <= 0) {
		Disconnect("RecvBuffer Overflow - Malicious packet or buffer exhaustion");
		return;
	}

	auto self(shared_from_this());

	// Strand를 통해 바인딩하여 멀티스레드 경합 없이 안전하게 핸들러 실행
	socket_.async_read_some(
		boost::asio::buffer(recvBuffer_.WritePos(), recvBuffer_.FreeSize()),
		boost::asio::bind_executor(
			strand_,
			[this, self](boost::system::error_code ec, std::size_t bytesTransferred) {
				if (!isConnected_.load()) return;

				if (!ec) {
					recvBuffer_.OnWrite(static_cast<int32_t>(bytesTransferred));

					bool parseError = false;
					int32_t processLen = ProcessPacketStream(parseError);

					if (processLen > 0) {
						recvBuffer_.OnRead(processLen);
					}

					// 비정상 패킷 감지 시 즉시 세션 차단
					if (parseError) {
						Disconnect("Corrupted Packet Header Detected");
						return;
					}

					// 다음 데이터 수신 대기
					DoRead();
				}
				else {
					Disconnect(ec.message());
				}
			}
		)
	);
}

int32_t Session::ProcessPacketStream(bool& outError) {
	outError = false;
	int32_t processLen = 0;

	while (true) {
		int32_t dataSize = recvBuffer_.DataSize() - processLen;

		// 헤더 크기보다 작으면 다음 수신 대기
		if (dataSize < sizeof(PacketHeader)) {
			break;
		}

		// ★ Fix 5 적용: std::memcpy로 역직렬화하여 메모리 정렬 크래시(Alignment Fault) 방지
		PacketHeader header;
		std::memcpy(&header, recvBuffer_.ReadPos() + processLen, sizeof(PacketHeader));

		// 패킷 크기 유효성 검사 (헤더 크기 미만이거나 최대 허용 크기 초과 시 에러)
		if (header.size < sizeof(PacketHeader) || header.size > 8192) {
			outError = true;
			break; // ★ Fix 1 적용: 0을 리턴하지 않고 현재까지 처리된 바이트(processLen) 보존
		}

		// 패킷 몸통(Body)이 아직 덜 도착했으면 대기
		if (dataSize < header.size) {
			break;
		}

		// 정상 패킷 전달
		OnRecvPacket(recvBuffer_.ReadPos() + processLen, header.size);
		processLen += header.size;
	}

	return processLen;
}

void Session::Send(SendBufferRef sendBuffer) {
	if (!isConnected_.load() || !sendBuffer) return;

	auto self(shared_from_this());

	// ★ Strand를 통해 send 작업을 해당 세션 전용 큐로 전달 (Mutex Lock 필요 없음!)
	boost::asio::post(
		strand_,
		[this, self, sendBuffer]() {
			RegisterSend(sendBuffer);
		}
	);
}

void Session::RegisterSend(SendBufferRef sendBuffer) {
	sendQueue_.push(sendBuffer);

	if (!isWriting_) {
		isWriting_ = true;
		DoWrite();
	}
}

void Session::DoWrite() {
	auto self(shared_from_this());

	std::vector<boost::asio::const_buffer> buffersToDraw;

	// Send 큐에 모인 모든 패킷을 모아서 한 번에 전달 (Gather I/O)
	while (!sendQueue_.empty()) {
		SendBufferRef buffer = sendQueue_.front();
		sendQueue_.pop();

		pendingSendVector_.push_back(buffer);
		buffersToDraw.push_back(boost::asio::buffer(buffer->Buffer(), buffer->AllocSize()));
	}

	if (buffersToDraw.empty()) {
		isWriting_ = false;
		return;
	}

	boost::asio::async_write(
		socket_,
		buffersToDraw,
		boost::asio::bind_executor(
			strand_,
			[this, self](boost::system::error_code ec, std::size_t /*bytesTransferred*/) {
				pendingSendVector_.clear(); // 전송 완료된 메모리 해제

				if (!isConnected_.load()) {
					isWriting_ = false;
					return;
				}

				if (!ec) {
					if (!sendQueue_.empty()) {
						DoWrite(); // 대기 중인 새 패킷 계속 전송
					}
					else {
						isWriting_ = false;
					}
				}
				else {
					isWriting_ = false;
					Disconnect(ec.message());
				}
			}
		)
	);
}