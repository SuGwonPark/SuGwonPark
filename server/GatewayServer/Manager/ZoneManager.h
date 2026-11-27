#pragma once
#include "pch.h"
#include "ServerCore/Network/SendBuffer.h"

class ZoneServerSession; // 게이트웨이가 내부 Zone 서버와 맺은 TCP 커넥션 세션
using ZoneServerSessionRef = std::shared_ptr<ZoneServerSession>;

class ZoneManager
{
  public:
    static ZoneManager* GetInstance()
    {
        static ZoneManager instance;
        return &instance;
    }

    // 존, 채널 생성
    void RegisterZone(uint32_t zoneID, uint32_t channelID, uint32_t maxPlayer, ZoneServerSessionRef session);
    // 존, 채널 해제
    void UnregisterZone(uint32_t zoneID, uint32_t channelID, ZoneServerSessionRef session);

    // 클라이언트 패킷에 sessionId 헤더를 감싸서 타겟 Zone으로 포워딩
    void SendToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionId, SendBufferRef clientPacketBuffer);

    // 클라이언트 연결 종료를 담당 Zone에 통보
    void SendDisconnectToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionId);

    void SendInternalToZone(
        uint32_t zoneID,
        uint32_t channelID,
        uint64_t sessionID,
        uint16_t packetID,
        const void* payload,
        uint32_t playloadSize);

    // 입장할 채널을 고르고 인원 +1 (정원이 남은 채널 중 번호가 가장 작은 것, 전부 찼으면 가장 한산한 것)
    // 채널 확인
    std::optional<uint32_t> AcquireChannel(uint32_t zoneID);
    // 채널 인원 -1 (접속 종료 / 포탈 이동)
    void ReleaseChannel(uint32_t zoneID, uint32_t channelID);

    

  private:
    ZoneManager() = default;
    ~ZoneManager() = default;

  private:
    std::mutex lock_;
    // 존 서버 갯수
    std::unordered_map<uint64_t, ZoneServerSessionRef> zoneServers_;

    // 채널 정원 / 현재 인원 [MakeZoneKey, 값] (lock으로 보호)
    // 채널 유저 입장 가능 수
    std::unordered_map<uint64_t, uint32_t> channelMax_;
    // 채널 유저 수 
    std::unordered_map<uint64_t, uint32_t> channelUsers_;
};