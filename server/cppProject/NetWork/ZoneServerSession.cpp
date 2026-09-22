#include "pch.h"
#include "Network/ZoneServerSession.h"
#include "Manager/ZoneManager.h"
#include "Manager/SendBufferManager.h"
#include "Manager/SessionManager.h"   // sessionId -> ClientProxySession 찾기용 (신규)

ZoneServerSession::ZoneServerSession(uint32_t zoneId, net::io_context& ioc)
	: zoneId_(zoneId)
	, socket_(ioc)
	, strand_(net::make_strand(ioc)) {
}

ZoneServerSession::~ZoneServerSession() {
	Close();
}

void ZoneServerSession::Start() {
	isConnected_.store(true);
	DoRead();
}

void ZoneServerSession::Close() {
	bool expected = true;
	if (isConnected_.compare_exchange_strong(expected, false)) {
		net::post(strand_, [self = shared_from_this()]() {
			boost::system::error_code ec;
			self->socket_.shutdown(tcp::socket::shutdown_both, ec);
			self->socket_.close(ec);

			// 내부 백본 연결이 끊기면 라우팅 테이블에서도 제거
			ZoneManager::GetInstance()->UnregisterZone(self->zoneId_);
			});
	}
}

void ZoneServerSession::DoRead() {
	auto self = shared_from_this();

	size_t freeSize = recvBuffer_.size() - writePos_;
	if (freeSize == 0) {
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

void ZoneServerSession::OnRead(const boost::system::error_code& ec, size_t bytesTransferred) {
	if (!ec) {
		writePos_ += bytesTransferred;
		ProcessPackets();
		DoRead();
	}
	else {
		Close();
	}
}

void ZoneServerSession::ProcessPackets() {
	while (true) {
		size_t dataSize = writePos_ - readPos_;
		if (dataSize < sizeof(InternalPacketHeader)) {
			break; // 내부 백본 헤더(12바이트) 미만
		}

		InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(&recvBuffer_[readPos_]);
		if (dataSize < header->size) {
			break; // 아직 다 안 옴
		}

		HandleInternalPacket(&recvBuffer_[readPos_], header->size);
		readPos_ += header->size;
	}

	if (readPos_ == writePos_) {
		readPos_ = 0;
		writePos_ = 0;
	}
}

void ZoneServerSession::HandleInternalPacket(uint8_t* buffer, uint16_t size) {
	InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(buffer);

	uint16_t payloadSize = size - sizeof(InternalPacketHeader);
	uint8_t* payload = buffer + sizeof(InternalPacketHeader);

	// Zone이 이 응답을 누구한테 보내는 건지는 header->sessionId로만 알 수 있음
	// → SessionManager에서 실제 ClientProxySession을 찾아서 그대로 전달
	ClientProxySessionRef target = SessionManager::GetInstance()->FindSession(header->sessionId);
	if (!target) return; // 이미 끊긴 클라이언트면 조용히 버림

	SendBufferRef sendBuffer = SendBufferManager::Open(payloadSize);
	sendBuffer->Write(payload, payloadSize);
	SendBufferManager::Close(payloadSize);

	target->Send(sendBuffer);
}

void ZoneServerSession::Send(SendBufferRef sendBuffer) {
	if (!isConnected_.load()) return;

	auto self = shared_from_this();
	net::post(strand_, [this, self, sendBuffer]() {
		bool isWriting =