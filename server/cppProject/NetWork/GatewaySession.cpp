#include "pch.h"
#include "GatewaySession.h"
#include "Manager/SendBufferManager.h"

void GatewaySession::Connect(net::io_context& ioc, const std::string& host, uint16_t port) {
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
				OnRead(ec, bytesTransferred);
			}
		)
	);
}

void GatewaySession::OnRead(const boost::system::error_code& ec, size_t bytesTransferred) {
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

void GatewaySession::ProcessPackets() {
	while (true) {
		size_t dataSize = writePos_ - readPos_;
		if (dataSize < sizeof(PacketHeader)) break;

		PacketHeader* header = reinterpret_cast<PacketHeader*>(&recvBuffer_[readPos_]);
		if (dataSize < header->size) break;

		// TODO: Gateway -> Zone 방향 제어 패킷은 아직 정의된 게 없어서 우선 건너뜀
		readPos_ += header->size;
	}

	if (readPos_ == writePos_) {
		readPos_ = 0;
		writePos_ = 0;
	}
}

void GatewaySession::Send(const RES_ZoneLeaveCompletedPacket& pkt) {
	if (!isConnected_.load()) return;

	SendBufferRef sendBuffer = SendBufferManager::Open(sizeof(pkt));
	sendBuffer->Write(&pkt, sizeof(pkt));
	SendBufferManager::Close(sizeof(pkt));

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
				OnWrite(ec, bytesTransferred);
			}
		)
	);
}

void GatewaySession::OnWrite(const boost::system::error_code& ec, size_t /*bytesTransferred*/) {
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