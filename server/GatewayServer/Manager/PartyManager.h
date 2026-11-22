#pragma once
#include "pch.h"
#include "Network/ClientProxySession.h"
#include "ServerCore/Protocol/VarEnum.h"

// 파티 관리 (Gateway 중앙). 파티원이 서로 다른 존에 있어도 파티 유지
// 여러 세션 strand에서 동시에 호출되므로 mutex 하나로 보호
// 잠금 순서: PartyManager → SessionManager (반대 방향 호출 없음)

struct Party
{
    uint64_t partyID = 0;
    uint64_t leaderID = 0;
    std::vector<uint64_t> members; // 가입 순서 대로(playerID)
};

class PartyManager
{
  public:
    static PartyManager* GetInstance()
    {
        static PartyManager inst;
        return &inst;
    }

    // 클라 파티 패킷 처리 (크기 검사 + 분기)
    void HandlePacket(const ClientProxySessionRef& session, uint16_t packetID, uint8_t* packet, uint16_t size);

    // 접속 종료 : 받은 초대 정리 + 파티 탈퇴
    void onDisconnect(uint64_t playerID);

    // 존 입장 시 존에 파티 정보 전달
    void SyncToZone(const ClientProxySessionRef& session);

  private:
    void Invite(const ClientProxySessionRef& inviter, uint64_t targetID);
    void Answer(const ClientProxySessionRef& target, uint64_t inviterID, bool accept);
    void Leave(const ClientProxySessionRef& session);
    void Kick(const ClientProxySessionRef& leader, uint64_t targetID);
    void ChangeLeader(const ClientProxySessionRef& leader, uint64_t targetID);

    Party* FindPartyOf(uint64_t playerID);

    void RemoveMember(Party& party, uint64_t playerID);
    void BroadcastInfo(const Party& party);

    static void SendInfo(
        const ClientProxySessionRef& session, const Party* party, const std::vector<PartyMemberInfo>& members);
    static void SendNoParty(const ClientProxySessionRef& session);
    static void SendResult(const ClientProxySessionRef& session, PartyResult result);
    static void SendZoneSync(const ClientProxySessionRef& session, uint64_t partyID);

  private:
    std::mutex lock_;
    uint64_t nextPartyID_ = 1;
    // 실제 파티 정보 [partyID, Party]
    std::unordered_map<uint64_t, Party> parties_;
    // 파티 맴버 [playerID,partyID]
    std::unordered_map<uint64_t, uint64_t> memberParty_;
    // 파티 초대 목록 (초대 받은 사람, (초대자 ID, 초대 유효 시간))
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::chrono::steady_clock::time_point>> invites_;
};
