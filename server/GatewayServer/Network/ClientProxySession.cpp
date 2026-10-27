#include "pch.h"
#include "Network/ClientProxySession.h"
#include "Manager/ZoneManager.h"   
#include "Manager/SessionManager.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/DB/GameDB.h"
#include "Game/Account.h"


ClientProxySession::ClientProxySession(uint64_t sessionID, net::io_context& ioc)
	: sessionID_(sessionID)
	, socket_(ioc)
	, strand_(net::make_strand(ioc)) {
}

ClientProxySession::~ClientProxySession() {
	Close();
}

void ClientProxySession::Start() {
	isConnected_.store(true);
	// 클라이언트 연결 상태 확인용 헬퍼 함수
	EnableKeepAlive(socket_);
	DoRead();
}

void ClientProxySession::Close() {
	bool expected = true;
	if (isConnected_.compare_exchange_strong(expected, false)) {
		net::post(strand_, [self = shared_from_this()]() {
			boost::system::error_code ec;
			self->socket_.shutdown(tcp::socket::shutdown_both, ec);
			self->socket_.close(ec);

			// 세션 연결 종료
			SessionManager::GetInstance()->Unregister(self->sessionID_, self->GetUserUID());

			// zone서버 세션 종료
			ZoneManager::GetInstance()->SendDisconnectToZone(self->GetCurrentZoneID(), self->GetCurrentChannelID(), self->sessionID_);
			});
	}
}


void ClientProxySession::DoRead()
{
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
				if (!ec) {
					writePos_ += bytesTransferred;
					ProcessPackets();
					DoRead();
				}
				else {
					std::cout << "[Session " << sessionID_ << "] 연결 종료: " << ec.message() << std::endl;
					Close();
				}
			}
		)
	);
}

void ClientProxySession::ProcessPackets() {
	while (true) {
		size_t dataSize = writePos_ - readPos_;

		if (dataSize < sizeof(PacketHeader)) {
			break; // 최소 헤더 크기 미만
		}

		PacketHeader* header = reinterpret_cast<PacketHeader*>(&recvBuffer_[readPos_]);
		// 최소 헤더 크기 미만이거나 큰 패킷이면 강제 종료
		if (header->size < sizeof(PacketHeader) || header->size > Net::MAX_CLIENT_PACKET_SIZE) {
			Close();
			break;
		}

		// 온전한 패킷 1개가 아직 덜 도착함
		if (dataSize < header->size) {
			break;
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
	// 로그인/회원가입은 게임 월드(Zone)와 무관한 Gateway 레벨 처리이므로
	// Zone으로 보내지 않고 여기서 바로 DB 조회 후 응답한다.
	switch (packetId) {
	case PKT_C_LOGIN:
		Account::GetInstance()->Login(
			shared_from_this(), reinterpret_cast<REQ_LoginPacket*>(packetPtr));
		return;
	case PKT_C_CHARACTER_LIST:
		Account::GetInstance()->SendCharacterList(shared_from_this());
		return;

	case PKT_C_SELECT_CHARACTER:
		if (packetSize < sizeof(REQ_SelectCharacterPacket)) return;
		Account::GetInstance()->SelectCharacter(shared_from_this(),
			reinterpret_cast<REQ_SelectCharacterPacket*>(packetPtr)->playerID);
		return;
	case PKT_C_READY_TO_SPAWN: {
		if (GetState() != SessionState::InGame) return;
		if (packetSize < sizeof(REQ_ReadyToSpawnPacket)) return;
		reinterpret_cast <REQ_ReadyToSpawnPacket*>(packetPtr)->playerID = GetPlayerID();
		break;
	}
    case PKT_C_CREATE_CHARACTER: {
    
        if (packetSize < sizeof(REQ_CreateCharacterPacket))
            return;
        Account::GetInstance()->CreateCharacter(
            shared_from_this(), reinterpret_cast<REQ_CreateCharacterPacket*>(packetPtr));
        return;
    }
	default:
		break;
	}

	// 게임 중이 아닌 세션은= 패킷을 Zone으로 보내지 않는다 (캐릭터 선택부터 Zone으로 보냄)
	if (GetState() != SessionState::InGame) return;

	// Zone 서버로 리턴하기위해 존재
	SendBufferRef sendBuffer = SendBufferManager::Make(packetPtr, packetSize);

	uint32_t zoneID = GetCurrentZoneID();
	uint32_t channelID = GetCurrentChannelID();
	ZoneManager::GetInstance()->SendToZone(zoneID, channelID, sessionID_, sendBuffer);
}

void ClientProxySession::OnZone1LeaveCompleted(uint32_t nextZoneID) {
	auto channelID = ZoneManager::GetInstance()->PickChannel(nextZoneID);
	if (!channelID) {
		// 목적지 Zone이 현재 하나도 등록돼 있지 않음. 플레이어는 이미 옛 Zone에서 빠졌으므로 연결을 끊는다
		std::cerr << "[Portal] 목적지 Zone 없음: " << nextZoneID << std::endl;
		Close();
		return;
	}

	SetCurrentChannelID(*channelID);
	SetCurrentZoneID(nextZoneID);

	// 클라이언트에 맵 전환 지시
	RES_ZoneChangePacket pkt{};
	pkt.header.id = PKT_S_ZONE_CHANGE;
	pkt.header.size = sizeof(pkt);
	pkt.zoneID = nextZoneID;
	pkt.channelID = *channelID;
	Send(SendBufferManager::Make(&pkt, sizeof(pkt)));
}

void ClientProxySession::Send(SendBufferRef sendBuffer) {
	if (!sendBuffer || !isConnected_.load()) return;

	auto self = shared_from_this();
	net::post(strand_, [this, self, sendBuffer]() {
		if (!isConnected_.load()) return;

		const size_t size = sendBuffer->AllocSize();
		if (sendQueueBytes_ + size > MAX_SEND_QUEUE_BYTES) {
			std::cerr << "[Session " << sessionID_ << "] 송신 큐 초과(" << sendQueueBytes_
				<< " bytes), 연결 종료" << std::endl;
			Close();
			return;
		}

		bool isWriting = !sendQueue_.empty();
		sendQueue_.push(sendBuffer);
		sendQueueBytes_ += size;
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
				if (!ec) {
					// SendByte 값 감소 처리
					sendQueueBytes_ -= sendQueue_.front()->AllocSize();
					sendQueue_.pop();
					if (!sendQueue_.empty()) {
						DoWrite();
					}
				}
				else {
					Close();
				}
			}
		)
	);
}
