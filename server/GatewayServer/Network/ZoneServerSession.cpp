#include "pch.h"
#include "Network/ZoneServerSession.h"

#include "Manager/ZoneManager.h"
#include "Manager/SessionManager.h" // sessionId -> ClientProxySession 찾기용 (신규)
#include "Manager/PartyManager.h"

#include "ServerCore/Manager/SendBufferManager.h"

ZoneServerSession::ZoneServerSession(net::io_context& ioc) : socket_(ioc), strand_(net::make_strand(ioc)) {}

ZoneServerSession::~ZoneServerSession()
{
    Close();
}

void ZoneServerSession::Start()
{
    boost::system::error_code ignore;
    socket_.set_option(tcp::no_delay(true), ignore);

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
                ZoneManager::GetInstance()->RegisterZone(e.zoneID, e.channelID, e.maxPlayer, shared_from_this());
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
            case PKT_INTERNAL_PARTY_MEMBER_STAT:
                ok = HandlePartyMemberStat(packet, ph->size);
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

    // sessionID 배열 (패킷 버퍼가 8바이트 정렬이 아닐 수 있으므로 복사해서 사용)
    std::vector<uint64_t> sessionIDs(mh->sessionCount);
    std::memcpy(sessionIDs.data(), ids, idsBytes);

    // 락 한 번으로 대상 세션을 모두 찾고, 락 밖에서 전송
    std::vector<ClientProxySessionRef> targets;
    SessionManager::GetInstance()->FindSessions(sessionIDs.data(), sessionIDs.size(), targets);

    // 세션 ID를 하나씩 찾아서 보내기 (멀티캐스트)
    for (const auto& target : targets)
        target->Send(sendBuffer);

    return true;
}

bool ZoneServerSession::HandlePartyMemberStat(uint8_t* buffer, uint16_t size)
{
    if (size < sizeof(InternalPacketHeader) + sizeof(PartyMemberStat))
        return false; // 스트림 깨짐

    auto* stat = reinterpret_cast<PartyMemberStat*>(buffer + sizeof(InternalPacketHeader));
    PartyManager::GetInstance()->OnMemberStat(*stat);
    return true;
}

void ZoneServerSession::Send(SendBufferRef sendBuffer)
{
    if (!isConnected_.load())
        return;

    auto self = shared_from_this();
    net::post(strand_, [this, self, sendBuffer]() {
        sendQueue_.push(sendBuffer);

        // 로그용
        ++statPushed_;                                              // 추가
        statQueueMax_ = std::max(statQueueMax_, sendQueue_.size()); // 추가

        if (!isWriting_)
            DoWrite();
    });
}

void ZoneServerSession::DoWrite()
{
    auto self = shared_from_this();

    // 큐에 쌓인 패킷을 최대 MAX_WRITE_BATCH개까지 한 번의 async_write로 보낸다
    writing_.clear();
    writeBufs_.clear();
    while (!sendQueue_.empty())
    {
        writing_.push_back(std::move(sendQueue_.front()));
        sendQueue_.pop();
        writeBufs_.emplace_back(writing_.back()->Buffer(), writing_.back()->AllocSize());
    }

    isWriting_ = true;

    net::async_write(
        socket_, writeBufs_,
        net::bind_executor(strand_, [this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
            if (ec)
            {
                isWriting_ = false;
                Close();
                return;
            }
            statWritten_ += writing_.size();
            statBytes_ += bytesTransferred;

            writing_.clear();
            ReportSendStat("Gateway→Zone"); // 추가

            if (!sendQueue_.empty())
                DoWrite();
            else
                isWriting_ = false;
        }));
}

// [측정] 5초마다 송신 큐 상태 출력. 대기 최대가 계속 커지면 이 연결이 병목
void ZoneServerSession::ReportSendStat(const char* label)
{
    const auto now = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(now - statLast_).count();
    if (sec < 5.0)
        return;
    std::string line = std::string("[Net] ") + label + " | 초당 넣음 " +
                       std::to_string(static_cast<int64_t>(statPushed_ / sec)) + ", 초당 보냄 " +
                       std::to_string(static_cast<int64_t>(statWritten_ / sec)) + " (" +
                       std::to_string(static_cast<int64_t>(statBytes_ / sec / 1024)) + " KB/s)" + " | 대기 최대 " +
                       std::to_string(statQueueMax_) + "개, 현재 " + std::to_string(sendQueue_.size()) + "개\n";
    std::cout << line;
    statLast_ = now;
    statQueueMax_ = 0;
    statPushed_ = statWritten_ = statBytes_ = 0;
}