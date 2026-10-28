#include "pch.h"
#include "Network/ZoneServerSession.h"
#include "Manager/ZoneManager.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "Manager/SessionManager.h" // sessionId -> ClientProxySession 찾기용 (신규)

ZoneServerSession::ZoneServerSession(net::io_context& ioc) : socket_(ioc), strand_(net::make_strand(ioc)) {}

ZoneServerSession::~ZoneServerSession()
{
    Close();
}

void ZoneServerSession::Start()
{
    isConnected_.store(true);
    DoRead();
}

void ZoneServerSession::Close()
{
    bool expected = true;
    if (isConnected_.compare_exchange_strong(expected, false))
    {
        net::post(strand_, [self = shared_from_this()]() {
            boost::system::error_code ec;
            self->socket_.shutdown(tcp::socket::shutdown_both, ec);
            self->socket_.close(ec);

            std::unordered_set<uint64_t> deadKeys;
            // 모든 존 채널 확인
            for (const auto& e : self->zoneKeys_)
            {
                ZoneManager::GetInstance()->UnregisterZone(e.zoneID, e.channelID, self);
                deadKeys.insert(MakeZoneKey(e.zoneID, e.channelID));
            }

            // 이 Zone 서버에 있던 클라이언트들은 더 이상 갈 곳이 없으므로 연결을 끊는다
            auto orphans = SessionManager::GetInstance()->CollectSessionsInZones(deadKeys);
            if (!orphans.empty())
            {
                std::cerr << "[ZoneServerSession] Zone 서버 연결 끊김, 클라이언트 " << orphans.size() << "명 접속 종료"
                          << std::endl;
            }
            for (auto& session : orphans)
            {
                session->Close();
            }
        });
    }
}

void ZoneServerSession::DoRead()
{
    auto self = shared_from_this();

    size_t freeSize = recvBuffer_.size() - writePos_;
    if (freeSize == 0)
    {
        size_t dataSize = writePos_ - readPos_;
        if (dataSize > 0)
        {
            std::memmove(&recvBuffer_[0], &recvBuffer_[readPos_], dataSize);
        }
        readPos_ = 0;
        writePos_ = dataSize;
        freeSize = recvBuffer_.size() - writePos_;
    }

    socket_.async_read_some(
        net::buffer(&recvBuffer_[writePos_], freeSize),
        net::bind_executor(strand_, [this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
            if (!ec)
            {
                writePos_ += bytesTransferred;
                ProcessPackets();
                DoRead();
            }
            else
            {
                Close();
            }
        }));
}

void ZoneServerSession::ProcessPackets()
{
    while (true)
    {
        size_t dataSize = writePos_ - readPos_;

        // 처음 연결시 1번만
        if (!handshakeDone_)
        {
            if (dataSize < sizeof(ZoneHandshakePacket))
                break;

            ZoneHandshakePacket* hs = reinterpret_cast<ZoneHandshakePacket*>(&recvBuffer_[readPos_]);
            if (dataSize < hs->header.size)
                break;

            // 크기 검증 : 헤더에 적힌 count와 크기가 맞는지 확인
            size_t expected = sizeof(ZoneHandshakePacket) + hs->count * sizeof(ZoneKeyEntry);
            if (hs->header.id != PKT_ZONE_HANDSHAKE || hs->header.size != expected)
            {
                Close();
                return;
            }

            auto* list = reinterpret_cast<ZoneKeyEntry*>(&recvBuffer_[readPos_] + sizeof(ZoneHandshakePacket));
            zoneKeys_.assign(list, list + hs->count);

            for (const auto& e : zoneKeys_)
            {
                ZoneManager::GetInstance()->RegisterZone(e.zoneID, e.channelID, shared_from_this());
            }

            std::cout << "Zone 서버 등록: 인스턴스 " << zoneKeys_.size() << "개" << std::endl;

            handshakeDone_ = true;
            readPos_ += hs->header.size;
            continue;
        }

        // 공통 헤더(크기, id)부터
        if (dataSize < sizeof(PacketHeader))
            break;

        PacketHeader* ph = reinterpret_cast<PacketHeader*>(&recvBuffer_[readPos_]);
        if (ph->size < sizeof(PacketHeader))
        {
            Close();
            return;
        } // 스트림 깨짐

        if (dataSize < ph->size)
            break; // 아직 다 안 옴

        uint8_t* packet = &recvBuffer_[readPos_];
        bool ok = true;
        switch (ph->id)
        {
            case PKT_INTERNAL_MULTICAST:
                ok = HandleMulticastPacket(packet, ph->size);
                break;
            default:
                if (ph->size < sizeof(InternalPacketHeader))
                {
                    ok = false;
                    break;
                }
                HandleInternalPacket(packet, ph->size);
                break;
        }
        if (!ok)
        {
            Close();
            return;
        }

        readPos_ += ph->size;
    }

    if (readPos_ == writePos_)
    {
        readPos_ = 0;
        writePos_ = 0;
    }
}

void ZoneServerSession::HandleInternalPacket(uint8_t* buffer, uint16_t size)
{
    InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(buffer);

    uint16_t payloadSize = size - sizeof(InternalPacketHeader);
    uint8_t* payload = buffer + sizeof(InternalPacketHeader);

    // Zone이 이 응답을 누구한테 보내는 건지는 header->sessionID로만 알 수 있음
    // → SessionManager에서 실제 ClientProxySession을 찾아서 그대로 전달
    ClientProxySessionRef target = SessionManager::GetInstance()->FindSession(header->sessionID);
    if (!target)
        return; // 이미 끊긴 클라이언트면 조용히 버림

    //  Zone 이동 완료 신호면 여기서 완료 처리
    if (payloadSize >= sizeof(PacketHeader))
    {
        PacketHeader* inner = reinterpret_cast<PacketHeader*>(payload);
        if (inner->id == PKT_S_ZONE_LEAVE_COMPLETED)
        {
            auto* ack = reinterpret_cast<RES_ZoneLeaveCompletedPacket*>(payload);
            target->OnZone1LeaveCompleted(ack->nextZoneID);
            return;
        }
    }

    SendBufferRef sendBuffer = SendBufferManager::Open(payloadSize);
    sendBuffer->Write(payload, payloadSize);
    SendBufferManager::Close(payloadSize);

    target->Send(sendBuffer);
}

// 형식이 잘못됐으면 false (호출한 쪽에서 연결 종료)
bool ZoneServerSession::HandleMulticastPacket(uint8_t* buffer, uint16_t size)
{
    if (size < sizeof(InternalMulticastHeader))
        return false;
    auto* mh = reinterpret_cast<InternalMulticastHeader*>(buffer);

    const size_t idsBytes = static_cast<size_t>(mh->sessionCount) * sizeof(uint64_t);
    if (sizeof(InternalMulticastHeader) + idsBytes + sizeof(PacketHeader) > size)
        return false;

    //[ InternalMulticastHeader ][ sessionID × N ][ 클라이언트 패킷 원본 ]
    const uint8_t* ids = buffer + sizeof(InternalMulticastHeader);
    const uint8_t* payload = ids + idsBytes;
    const uint32_t payloadSize = static_cast<uint32_t>(size - sizeof(InternalMulticastHeader) - idsBytes);

    // 클라에게 보낼 버퍼는 한 번만 만들어 모두 공유 (보내는 동안 읽기만 하므로 안전)
    // 실질적으로 클라에 보내질 패킷 데이터
    SendBufferRef sendBuffer = SendBufferManager::Make(payload, payloadSize);

    // 세션 ID를 하나씩 찾아서 보내기 (멀티캐스트)
    for (uint16_t i = 0; i < mh->sessionCount; ++i)
    {
        uint64_t sessionID;
        // 한명의 세션 ID를 꺼내서 SessionManager에서 찾아서 보내기 (멀티캐스트)
        std::memcpy(&sessionID, ids + i * sizeof(uint64_t), sizeof(sessionID)); 
        if (auto target = SessionManager::GetInstance()->FindSession(sessionID))
            target->Send(sendBuffer);
    }
    return true;
}

void ZoneServerSession::Send(SendBufferRef sendBuffer)
{
    if (!isConnected_.load())
        return;

    auto self = shared_from_this();
    net::post(strand_, [this, self, sendBuffer]() {
        bool isWriting = !sendQueue_.empty();
        sendQueue_.push(sendBuffer);

        if (!isWriting)
        {
            DoWrite();
        }
    });
}

void ZoneServerSession::DoWrite()
{
    auto self = shared_from_this();
    SendBufferRef sendBuffer = sendQueue_.front();

    net::async_write(
        socket_, net::buffer(sendBuffer->Buffer(), sendBuffer->AllocSize()),
        net::bind_executor(strand_, [this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
            if (!ec)
            {
                sendQueue_.pop();
                if (!sendQueue_.empty())
                {
                    DoWrite();
                }
            }
            else
            {
                Close();
            }
        }));
}
