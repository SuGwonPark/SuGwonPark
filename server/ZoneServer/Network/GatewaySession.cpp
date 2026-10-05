#include "pch.h"
#include "GatewaySession.h"
#include "Manager/SendBufferManager.h"

void GatewaySession::Connect(net::io_context& ioc, const std::string& host, uint16_t port, uint32_t zoneID, uint32_t channelID) {
	zoneID_ = zoneID;
	channelID_ = channelID;

	socket_ = std::make_unique<tcp::socket>(ioc);
	strand_ = std::make_unique<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));

	auto resolver = std::make_shared<tcp::resolver>(ioc);
	resolver->async_resolve(host, std::to_string(port),
		[this, resolver](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
			if (ec) {
				std::cerr << "[GatewaySession] 주소 확인 실패: " << ec.message() << std::endl;
				return;
			}
			net::async_connect(*socket_, endpoints,
				[this](boost::system::error_code ec, const tcp::endpoint&) {
					if (ec) {
						std::cerr << "[GatewaySession] Gateway 접속 실패: " << ec.message() << std::endl;
						return;
					}
					isConnected_.store(true);
					std::cout << "[GatewaySession] Gateway 접속 성공" << std::endl;

					ZoneHandshakePacket hs{};
					hs.header.id = PKT_ZONE_HANDSHAKE;
					hs.header.size = sizeof(hs);
					hs.zoneID = zoneID_;
					hs.channelID = channelID_;

					Send(SendBufferManager::Make(&hs, sizeof(hs)));
					DoRead();
				});
		});
}

void GatewaySession::DoRead() {
	if (!isConnected_.load()) return;

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

	socket_->async_read_some(
		net::buffer(&recvBuffer_[writePos_], freeSize),
		net::bind_executor(*strand_,
			[this](const boost::system::error_code& ec, size_t bytesTransferred) {
				if (!ec) {
					writePos_ += bytesTransferred;
					ProcessPackets();
					DoRead();
				}
				else {
					isConnected_.store(false);
					std::cerr << "[GatewaySession] Gateway 연결 끊김: " << ec.message() << std::endl;
				}
			}
		)
	);
}


void GatewaySession::ProcessPackets() {
	while (true) {
		size_t dataSize = writePos_ - readPos_;
		if (dataSize < sizeof(InternalPacketHeader)) break;

		InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(&recvBuffer_[readPos_]);
		if (dataSize < header->size) break;

		HandleInternalPacket(&recvBuffer_[readPos_], header->size);
		readPos_ += header->size;
	}

	if (readPos_ == writePos_) {
		readPos_ = 0;
		writePos_ = 0;
	}
}

void GatewaySession::Send(SendBufferRef sendBuffer) {
	if (!isConnected_.load()) return;

	net::post(*strand_, [this, sendBuffer]() {
		bool isWriting = !sendQueue_.empty();
		sendQueue_.push(sendBuffer);
		if (!isWriting) {
			DoWrite();
		}
		});
}

void GatewaySession::DoWrite() {
	SendBufferRef sendBuffer = sendQueue_.front();

	net::async_write(*socket_,
		net::buffer(sendBuffer->Buffer(), sendBuffer->AllocSize()),
		net::bind_executor(*strand_,
			[this](boost::system::error_code ec, size_t bytesTransferred) {
				if (!ec) {
					sendQueue_.pop();
					if (!sendQueue_.empty()) {
						DoWrite();
					}
				}
				else {
					isConnected_.store(false);
					std::cerr << "[GatewaySession] 송신 실패: " << ec.message() << std::endl;
				}
			}
		)
	);
}


void GatewaySession::HandleInternalPacket(uint8_t* buffer, uint16_t size) {
	InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(buffer);
	uint16_t payloadSize = size - sizeof(InternalPacketHeader);
	uint8_t* payload = buffer + sizeof(InternalPacketHeader);

	if (!zone_) return; // 존이 세팅 안됬으면 무시

	zone_->HandleClientPacket(header->sessionID, payload, payloadSize);
}