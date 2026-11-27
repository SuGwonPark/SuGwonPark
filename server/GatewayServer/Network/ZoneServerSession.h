#pragma once
#include "pch.h"
#include "GatewayConstants.h"
#include "ServerCore/Network/SendBuffer.h"
#include "ServerCore/Protocol/InternalPackets.h"

class ZoneServerSession : public std::enable_shared_from_this<ZoneServerSession>
{
  public:
    ZoneServerSession(net::io_context& ioc);
    ~ZoneServerSession();

    void Start();
    void Close();

    // Gateway -> ZoneServer 패킷 전송 (ZoneManager가 호출)
    void Send(SendBufferRef sendBuffer);

    tcp::socket& Socket()
    {
        return socket_;
    }

  private:
    void DoRead();
    void DoWrite();

    void ProcessPackets();
    // 유저에게 전달
    void HandleInternalPacket(uint8_t* buffer, uint16_t size);

    bool HandleMulticastPacket(uint8_t* buffer, uint16_t size);
    // zone이 보낸 파티원 스탯
    bool HandlePartyMemberStat(uint8_t* buffer, uint16_t size);

  private:
    std::vector<ZoneKeyEntry> zoneKeys_;

    bool handshakeDone_ = false; // 추가

    tcp::socket socket_;
    net::strand<net::io_context::executor_type> strand_;

    std::atomic<bool> isConnected_{false};

    // 수신 스트림 버퍼
    std::array<uint8_t, GatewayConst::ZONE_RECV_BUFFER_SIZE> recvBuffer_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;

    // 송신 큐
    std::queue<SendBufferRef> sendQueue_;

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

using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;