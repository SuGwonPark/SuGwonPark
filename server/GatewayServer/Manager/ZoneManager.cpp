#include "pch.h"
#include "Manager/ZoneManager.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "Network/ZoneServerSession.h"
#include "ServerCore/Protocol/InternalPackets.h"
#include "ServerCore/Utils/util.h"

// 존 생성
void ZoneManager::RegisterZone(uint32_t zoneID, uint32_t channelID, uint32_t maxPlayer, ZoneServerSessionRef session)
{
    std::lock_guard lock(lock_);
    const uint64_t key = MakeZoneKey(zoneID, channelID);
    zoneServers_[key] = session;
    channelMax_[key] = maxPlayer;
}

void ZoneManager::UnregisterZone(uint32_t zoneID, uint32_t channelID, ZoneServerSessionRef session)
{
    std::lock_guard<std::mutex> lock(lock_);
    auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));

    // 존 서버가 있으면서 세션이 같으면 제거
    if (it != zoneServers_.end() && it->second == session)
    {
        zoneServers_.erase(it);
    }
}

void ZoneManager::SendToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionID, SendBufferRef clientPacketBuffer)
{
    ZoneServerSessionRef targetSession = nullptr;
    {
        std::lock_guard<std::mutex> lock(lock_);
        auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));
        if (it != zoneServers_.end())
        {
            targetSession = it->second;
        }
    }

    // 존, 채널을 찾지 못하면 리턴
    if (!targetSession)
        return;

    InternalPacketHeader header{};
    header.header.id = PKT_INTERNAL_FORWARD;
    header.sessionID = sessionID;
    header.zoneID = zoneID;
    header.channelID = channelID;

    SendBufferRef wrapped =
        SendBufferManager::MakeWithPayload(header, clientPacketBuffer->Buffer(), clientPacketBuffer->AllocSize());
    if (!wrapped)
        return;

    targetSession->Send(wrapped);
}

void ZoneManager::SendDisconnectToZone(uint32_t zoneID, uint32_t channelID, uint64_t sessionID)
{
    // 세션 종료 알림 패킷을 해당 Zone으로 전송하여 메모리 정리 유도
    InternalPacketHeader header{};
    header.header.size = sizeof(InternalPacketHeader);
    header.header.id = PKT_INTERNAL_DISCONNECT; // DISCONNECT 패킷 ID
    header.sessionID = sessionID;
    header.zoneID = zoneID;
    header.channelID = channelID;

    SendBufferRef sendBuffer = SendBufferManager::Make(&header, sizeof(header));

    std::lock_guard<std::mutex> lock(lock_);
    auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));
    if (it != zoneServers_.end())
    {
        it->second->Send(sendBuffer);
    }
}

void ZoneManager::SendInternalToZone(
    uint32_t zoneID,
    uint32_t channelID,
    uint64_t sessionID,
    uint16_t packetID,
    const void* payload,
    uint32_t playloadSize)
{
    ZoneServerSessionRef target;
    {
        std::lock_guard<std::mutex> lock(lock_);
        auto it = zoneServers_.find(MakeZoneKey(zoneID, channelID));
        if (it != zoneServers_.end())
            target = it->second;
    }
    if (!target)
        return;

    InternalPacketHeader header{};
    header.header.id = packetID;
    header.sessionID = sessionID;
    header.zoneID = zoneID;
    header.channelID = channelID;

    if (SendBufferRef buf = SendBufferManager::MakeWithPayload(header, payload, playloadSize))
        target->Send(buf);
}

// 채널 확인
std::optional<uint32_t> ZoneManager::AcquireChannel(uint32_t zoneID)
{
    std::lock_guard<std::mutex> lock(lock_);

    std::optional<uint32_t> pick;      // 정원이 남은 채널 중 번호가 가장 적은 것
    std::optional<uint32_t> leastFull; // 전부 찼을 때: 인원이 가장 적은 것
    uint32_t leastUsers = UINT32_MAX;

    for (const auto& [key, session] : zoneServers_)
    {
        // 존 ID 불일치
        if (KeyToZoneID(key) != zoneID)
            continue;
        const uint32_t channelID = KeyToChannelID(key);
        const uint32_t users = channelUsers_[key];
        const uint32_t maxPlayer = channelMax_[key];

        const bool hasRoom = maxPlayer == 0 || users < maxPlayer;
        // 입장 가능 채널중 가장 작은 채널
        if (hasRoom && (!pick || channelID < *pick))
            pick = channelID;

        // 유저가 더 적은 채널 || 유저수가 같으면 채널ID가 낮은곳으로
        if (!leastFull || users < leastUsers || (users == leastUsers && channelID < *leastFull))
        {
            leastUsers = users;
            leastFull = channelID;
        }
    }

    if (!pick && leastFull)
    {
        // 모든 채널이 정원 초과: 입장은 막지 않고 가장 한산한 채널로
        std::cerr << "[Channel] 존 " << zoneID << " 전 채널 정원 초과, 채널 " << *leastFull << "로 입장 (" << leastUsers
                  << "명)" << std::endl;
        pick = leastFull;
    }

    // 채널 유저 증가
    if (pick)
        ++channelUsers_[MakeZoneKey(zoneID, *pick)];

    return pick;
}

// 채널 유저 해제
void ZoneManager::ReleaseChannel(uint32_t zoneID, uint32_t channelID) {
    std::lock_guard<std::mutex> lock(lock_);
    auto it = channelUsers_.find(MakeZoneKey(zoneID, channelID));
    if (it != channelUsers_.end() && it->second > 0)
        --it->second;
}
