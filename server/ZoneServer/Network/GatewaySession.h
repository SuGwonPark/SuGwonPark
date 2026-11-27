#pragma once
#include "pch.h"
#include "ServerCore/Protocol/InternalPackets.h"
#include "ServerCore/Network/SendBuffer.h"
#include "World/Zone.h"

// Zone 서버 -> Gateway로 향하는 연결 (ZoneServerSession의 반대편 끝).
// Zone에서 Gateway로 보내야 하는 제어 패킷(예: Zone 이동 완료 ACK)을 여기로 보낸다.
// 프로세스 생애주기 내내 하나만 존재하는 싱글턴이라 shared_from_this는 쓰지 않는다.
class GatewaySession
{
    enum
    {
        RECV_BUFFER_SIZE = 65536
    };

  public:
    static GatewaySession* GetInstance()
    {
        static GatewaySession inst;
        return &inst;
    }

    // Zone 프로세스 시작 시 한 번 호출해서 Gateway로 접속한다
    void Connect(net::io_context& ioc, const std::string& host, uint16_t port);

    // Zone -> Gateway 제어 패킷 전송 (strand로 직렬화됨)
    void Send(SendBufferRef sendBuffer);
    void AddZone(ZoneRef zone);

    // GatewayServer로 전송
    void SendToClient(uint64_t sessionID, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size);
    bool IsConnected() const
    {
        return isConnected_.load();
    }

    void MulticastToClients(
        const std::vector<uint64_t>& sessionIDs, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size);

    // Zone -> Gateway 내부 패킷 (클라 전달용이 아닌 제어 패킷)
    void SendInternal(uint16_t packetID, uint32_t zoneID, uint32_t channelID, const void* data, uint16_t size);

  private:
    GatewaySession() = default;

    void DoRead();
    void ProcessPackets();
    void HandleInternalPacket(uint8_t* buffer, uint16_t size);

    void DoWrite();

    void TryConnect();
    void OnConnected();
    void OnDisconnected(const std::string& reson);
    void ScheduleReconnect();

  private:
    std::unordered_map<uint64_t, ZoneRef> zones_;
    std::unique_ptr<tcp::socket> socket_;
    std::unique_ptr<net::strand<net::io_context::executor_type>> strand_;

    std::atomic<bool> isConnected_{false};

    std::array<uint8_t, RECV_BUFFER_SIZE> recvBuffer_{};
    size_t readPos_ = 0;
    size_t writePos_ = 0;

    std::queue<SendBufferRef> sendQueue_;

    // 재연결 시간
    static constexpr int RECONNECT_DELAY_SEC = 3;
    net::io_context* ioc_ = nullptr;
    std::string host_;
    uint64_t port_ = 0;
    std::unique_ptr<net::steady_timer> retryTimer_;
    uint32_t connGen_ = 0;

    bool isWriting_ = false;
    std::vector<SendBufferRef> writing_; // 지금 보내는 중인 묶음 (완료 전까지 버퍼 유지)
    std::vector<net::const_buffer> writeBufs_;
    
    // [측정] 송신 큐 부하 (strand 안에서만 접근). 5초마다 콘솔에 한 줄 출력
    size_t statQueueMax_ = 0;  // 대기 패킷 수 최대
    uint64_t statPushed_ = 0;  // 큐에 넣은 패킷 수
    uint64_t statWritten_ = 0; // 실제로 소켓에 쓴 패킷 수
    uint64_t statBytes_ = 0;   // 쓴 바이트
    std::chrono::steady_clock::time_point statLast_ = std::chrono::steady_clock::now();
    void ReportSendStat(const char* label);
};