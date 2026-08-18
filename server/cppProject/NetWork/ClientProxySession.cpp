#include "pch.h"
#include "ClientProxySession.h"
#include "Manager/ZoneManager.h"   // Zone 서버 통신 매니저 (SendToZone)
//#include "ChatManager.h"   // Chat 서버 통신 매니저 (SendToChatServer)


ClientProxySession::ClientProxySession(uint64_t sessionId, net::io_context& ioc)
	: sessionId_(sessionId)
	, socket_(ioc)
	, strand_(net::make_strand(ioc)) {
}

ClientProxySession::~ClientProxySession() {
	Close();
}

void ClientProxySession::Start() {
	isConnected_.store(true);
	DoRead();
}

void ClientProxySession::Close() {
	bool expected = true;
	if (isConnected_.compare_exchange_strong(expected, false)) {
		net::post(strand_, [self = shared_from_this()]() {
			boost::system::error_code ec;
			self->socket_.shutdown(tcp::socket::shutdown_both, ec);
			self->socket_.close(ec);

			// 연결 종료 시 내부 Zone 서버에 연결 끊김 통보
			ZoneManager::GetInstance()->SendDisconnectToZone(self->GetCurrentZoneId(), self->sessionId_);
			});
	}
}

void ClientProxySession::DoRead() {
	auto self = shared_from_this();

	// 수신 가능 버퍼 영역 계산
	size_t freeSize = recvBuffer_.size() - writePos_;
	if (freeSize == 0) {
		// 남은 공간이 없으면 데이터 앞으로 땡기기
		size_t dataSize = writePos_ - readPos_;
		if (dataSize > 0) {
			std::memmove(&recvBuffer_[0], &recvBuffer_[readPos_], dataSize);
		}
		readPos_ = 0;
		writePos_ = dataSize;
		freeSize = recvBuffer_.size() - writePos_;
	}

	socket_.async_read_some(
		net::buffer(&recvBuffer_[writePos_], freeSize),
		net::bind_executor(strand_,
			[this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
				OnRead(ec, bytesTransferred);
			}
		)
	);
}

void ClientProxySession::OnRead(const boost::system::error_code& ec, size_t bytesTransferred) {
	if (!ec) {
		writePos_ += bytesTransferred;
		ProcessPackets();
		DoRead();
	}
	else {
		Close();
	}
}

void ClientProxySession::ProcessPackets() {
	while (true) {
		size_t dataSize = writePos_ - readPos_;
		if (dataSize < sizeof(PacketHeader)) {
			break; // 최소 헤더 크기 미만
		}

		PacketHeader* header = reinterpret_cast<PacketHeader*>(&recvBuffer_[readPos_]);
		if (dataSize < header->size) {
			break; // 온전한 패킷 1개가 아직 덜 도착함
		}

		// 완성된 패킷 1개 라우팅 처리
		RoutePacket(header->id, &recvBuffer_[readPos_], header->size);
		readPos_ += header->size;
	}

	// 버퍼가 전부 비워졌으면 포인터 리셋
	if (readPos_ == writePos_) {
		readPos_ = 0;
		writePos_ = 0;
	}
}

void ClientProxySession::RoutePacket(uint16_t packetId, uint8_t* packetPtr, uint16_t packetSize) {
	// 1. TLS SendBuffer 풀에서 고속 할당
	SendBufferRef sendBuffer = SendBufferManager::Open(packetSize);
	sendBuffer->Write(packetPtr, packetSize);
	SendBufferManager::Close(packetSize);

	//// 2. 채팅 패킷은 ChatServer로 즉시 바이패스
	//if (packetId == PKT_C_CHAT || packetId == PKT_C_WHISPER) {
	//	ChatManager::GetInstance()->SendToChatServer(sessionId_, sendBuffer);
	//	return;
	//}

	// 3. 게임 패킷은 현재 담당 Zone 서버로 직행 (Zone 1 드레인이 끝날 때까지는 Zone 1으로 전달)
	uint32_t zoneId = GetCurrentZoneId();
	ZoneManager::GetInstance()->SendToZone(zoneId, sessionId_, sendBuffer);
}

void ClientProxySession::OnZone1LeaveCompleted(uint32_t nextZoneId) {
	// Zone 1의 Drain 및 저장이 끝났으므로 다음 패킷부터는 즉시 Zone 2로 라우팅
	SetCurrentZoneId(nextZoneId);
}

void ClientProxySession::Send(SendBufferRef sendBuffer) {
	if (!isConnected_.load()) return;

	auto self = shared_from_this();
	net::post(strand_, [this, self, sendBuffer]() {
		bool isWriting = !sendQueue_.empty();
		sendQueue_.push(sendBuffer);

		if (!isWriting) {
			DoWrite();
		}
		});
}

void ClientProxySession::DoWrite() {
	auto self = shared_from_this();
	SendBufferRef sendBuffer = sendQueue_.front();

	net::async_write(
		socket_,
		net::buffer(sendBuffer->Buffer(), sendBuffer->AllocSize()),
		net::bind_executor(strand_,
			[this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
				OnWrite(ec, bytesTransferred);
			}
		)
	);
}

void ClientProxySession::OnWrite(const boost::system::error_code& ec, size_t bytesTransferred) {
	if (!ec) {
		sendQueue_.pop();
		if (!sendQueue_.empty()) {
			DoWrite();
		}
	}
	else {
		Close();
	}
}