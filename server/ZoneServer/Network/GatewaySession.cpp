#include "pch.h"
#include "GatewaySession.h"
#include "ServerCore/Manager/SendBufferManager.h"

void GatewaySession::Connect(net::io_context& ioc, const std::string& host, uint16_t port)
{
    ioc_ = &ioc;
    host_ = host;
    port_ = port;
    strand_ = std::make_unique<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));
    retryTimer_ = std::make_unique<net::steady_timer>(*strand_); // 타이머 콜백도 strand에서 실행

    net::post(*strand_, [this]() { TryConnect(); });
}

void GatewaySession::TryConnect()
{
    const uint32_t gen = ++connGen_;

    // 새 연결 준비: 소켓 새로 만들고 버퍼/큐 비우기
    socket_ = std::make_unique<tcp::socket>(*ioc_);
    readPos_ = 0;
    writePos_ = 0;
    std::queue<SendBufferRef>().swap(sendQueue_);

    auto resolver = std::make_shared<tcp::resolver>(*ioc_);
    resolver->async_resolve(
        host_, std::to_string(port_),
        net::bind_executor(
            *strand_, [this, resolver, gen](boost::system::error_code ec, tcp::resolver::results_type endpoints) {
                if (gen != connGen_)
                    return;
                if (ec)
                {
                    std::cerr << "[GatewaySession] 주소 확인 실패: " << ec.message() << std::endl;
                    ScheduleReconnect();
                    return;
                }
                net::async_connect(
                    *socket_, endpoints,
                    net::bind_executor(*strand_, [this, gen](boost::system::error_code ec, const tcp::endpoint&) {
                        if (gen != connGen_)
                            return;
                        if (ec)
                        {
                            std::cerr << "[GatewaySession] Gateway 접속 실패: " << ec.message() << std::endl;
                            ScheduleReconnect();
                            return;
                        }
                        OnConnected();
                    }));
            }));
}

void GatewaySession::OnConnected()
{
    isConnected_.store(true);
    std::cout << "[GatewaySession] Gateway 접속 성공" << std::endl;

    // 핸드셰이크 (기존 코드 그대로)
    std::vector<ZoneKeyEntry> list;
    for (auto& [key, zone] : zones_)
    {
        ZoneKeyEntry c{};
        c.zoneID = zone->GetZoneID();
        c.channelID = zone->GetChannelID();
        list.push_back(c);
    }
    ZoneHandshakePacket hs{};
    hs.header.id = PKT_ZONE_HANDSHAKE;
    hs.count = static_cast<uint16_t>(list.size());
    if (SendBufferRef buf = SendBufferManager::MakeList(hs, list))
        Send(buf);

    DoRead();
}

void GatewaySession::OnDisconnected(const std::string& reason)
{
    // 읽기/쓰기 에러가 동시에 와도 한 번만 처리
    if (!isConnected_.exchange(false))
        return;
    std::cerr << "[GatewaySession] Gateway 연결 끊김: " << reason << std::endl;

    boost::system::error_code ec;
    socket_->close(ec);

    // Gateway가 없으면 클라이언트도 전부 없는 것 → 모든 Zone의 플레이어 저장 후 비우기
    for (auto& [key, zone] : zones_)
        zone->HandleGatewayLost();

    ScheduleReconnect();
}

void GatewaySession::ScheduleReconnect()
{
    std::cout << "[GatewaySession] " << RECONNECT_DELAY_SEC << "초 후 재접속 시도" << std::endl;
    retryTimer_->expires_after(std::chrono::seconds(RECONNECT_DELAY_SEC));
    retryTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (ec)
            return;
        TryConnect();
    });
}

void GatewaySession::MulticastToClients(
    const std::vector<uint64_t>& sessionIDs, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size)
{
    if (sessionIDs.empty())
        return;

    if (size >= 0xFFFF - sizeof(InternalMulticastHeader))
        return;

    // 내부 패킷 한 개의 최대 크기(uint16 = 65535) 안에 들어가도록 나눠서 보냄
    const size_t room = 0xFFFF - sizeof(InternalMulticastHeader) - size;
    // 한번에 들어갈 수 있는 sessionID 수
    const size_t perPacket = room / sizeof(uint64_t);
    if (perPacket == 0)
        return; // 클라 패킷이 너무 큼 (현실적으로 없음)

    // sessionIDs를 perPacket씩 나눠서 전송
    for (size_t start = 0; start < sessionIDs.size(); start += perPacket)
    {
        const size_t count = std::min(perPacket, sessionIDs.size() - start);
        const size_t idsBytes = count * sizeof(uint64_t);
        // total =  InternalMultiCast 기본 헤더 사이즈 + sessionID 배열 사이즈 + 클라 패킷 사이즈
        const size_t total = sizeof(InternalMulticastHeader) + idsBytes + size;

        // 내부 멀티캐스트 패킷 헤더 작성
        InternalMulticastHeader head{};
        head.header.id = PKT_INTERNAL_MULTICAST;
        head.header.size = static_cast<uint16_t>(total);
        head.zoneID = zoneID;
        head.channelID = channelID;
        head.sessionCount = static_cast<uint16_t>(count);

        SendBufferRef buf = SendBufferManager::Open(static_cast<uint32_t>(total));
        uint8_t* p = buf->Buffer();
        // internal header 복사
        std::memcpy(p, &head, sizeof(head));
        p += sizeof(head);
        // sessionID 배열 복사
        std::memcpy(p, sessionIDs.data() + start, idsBytes);
        p += idsBytes;
        std::memcpy(p, data, size);
        SendBufferManager::Close(static_cast<uint32_t>(total));

        Send(buf);
    }
}

void GatewaySession::DoRead()
{
    if (!isConnected_.load())
        return;

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

    socket_->async_read_some(
        net::buffer(&recvBuffer_[writePos_], freeSize),
        net::bind_executor(
            *strand_, [this, gen = connGen_](const boost::system::error_code& ec, size_t bytesTransferred) {
                if (gen != connGen_)
                    return; // 이전 연결의 콜백이면 무시
                if (!ec)
                {
                    writePos_ += bytesTransferred;
                    ProcessPackets();
                    DoRead();
                }
                else
                {
                    OnDisconnected(ec.message());
                }
            }));
}

void GatewaySession::ProcessPackets()
{
    while (true)
    {
        size_t dataSize = writePos_ - readPos_;
        if (dataSize < sizeof(InternalPacketHeader))
            break;

        InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(&recvBuffer_[readPos_]);
        if (dataSize < header->header.size)
            break;

        if (header->header.size < sizeof(InternalPacketHeader))
        {
            OnDisconnected("잘못된 패킷 크기: " + std::to_string(header->header.size));
            return;
        }

        HandleInternalPacket(&recvBuffer_[readPos_], header->header.size);
        readPos_ += header->header.size;
    }

    if (readPos_ == writePos_)
    {
        readPos_ = 0;
        writePos_ = 0;
    }
}

void GatewaySession::Send(SendBufferRef sendBuffer)
{
    if (!isConnected_.load())
        return;

    net::post(*strand_, [this, sendBuffer]() {
        bool isWriting = !sendQueue_.empty();
        sendQueue_.push(sendBuffer);
        if (!isWriting)
        {
            DoWrite();
        }
    });
}

void GatewaySession::DoWrite()
{
    SendBufferRef sendBuffer = sendQueue_.front();

    net::async_write(
        *socket_, net::buffer(sendBuffer->Buffer(), sendBuffer->AllocSize()),
        net::bind_executor(*strand_, [this, gen = connGen_](boost::system::error_code ec, size_t bytesTransferred) {
            if (gen != connGen_)
                return; // 이전 연결의 콜백이면 무시
            if (!ec)
            {
                sendQueue_.pop();
                if (!sendQueue_.empty())
                    DoWrite();
            }
            else
            {
                OnDisconnected(ec.message()); // 플래그만 내리지 말고 정리 경로로
            }
        }));
}

void GatewaySession::HandleInternalPacket(uint8_t* buffer, uint16_t size)
{
    InternalPacketHeader* header = reinterpret_cast<InternalPacketHeader*>(buffer);
    uint16_t payloadSize = size - sizeof(InternalPacketHeader);
    uint8_t* payload = buffer + sizeof(InternalPacketHeader);

    auto it = zones_.find(MakeZoneKey(header->zoneID, header->channelID));

    if (it == zones_.end())
        return; // 존이 세팅 안됬으면 무시

    // 연결 종료
    if (header->header.id == PKT_INTERNAL_DISCONNECT)
    {
        it->second->HandleDisconnect(header->sessionID);
        return;
    }

    it->second->HandleClientPacket(header->sessionID, payload, payloadSize);
}

void GatewaySession::AddZone(ZoneRef zone)
{
    zones_[MakeZoneKey(zone->GetZoneID(), zone->GetChannelID())] = zone;
}

void GatewaySession::SendToClient(
    uint64_t sessionID, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size)
{
    InternalPacketHeader header{};
    header.header.id = PKT_INTERNAL_FORWARD;
    header.sessionID = sessionID;
    header.zoneID = zoneID;
    header.channelID = channelID;

    SendBufferRef buf = SendBufferManager::MakeWithPayload(header, data, size);
    if (!buf)
        return;

    Send(buf);
}
