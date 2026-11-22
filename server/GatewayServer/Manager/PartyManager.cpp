#include "pch.h"
#include "Manager/PartyManager.h"
#include "Manager/SessionManager.h"
#include "Manager/ZoneManager.h"
#include "GatewayConstants.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/Protocol/InternalPackets.h"

void PartyManager::HandlePacket(const ClientProxySessionRef& session, uint16_t packetID, uint8_t* packet, uint16_t size)
{
    switch (packetID)
    {
        // 초대
        case PKT_C_PARTY_INVITE:
            if (size >= sizeof(REQ_PartyInvitePacket))
                Invite(session, reinterpret_cast<REQ_PartyInvitePacket*>(packet)->targetPlayerID);
            break;
            // 수락/거절
        case PKT_C_PARTY_ANSWER:
            if (size >= sizeof(REQ_PartyAnswerPacket))
            {
                auto* req = reinterpret_cast<REQ_PartyAnswerPacket*>(packet);
                Answer(session, req->inviterID,req->accept);
            }
            break;
            // 탈퇴
        case PKT_C_PARTY_LEAVE:
            if (size >= sizeof(REQ_PartyLeavePacket))
                Leave(session);
            break;

        case PKT_C_PARTY_KICK:
            if (size >= sizeof(REQ_PartyKickPacket))
            {
                auto* req = reinterpret_cast<REQ_PartyKickPacket*>(packet);
                Kick(session, req->targetPlayerID);
            }
            break;
        case PKT_C_PARTY_CHANGE_LEADER:
            if (size >= sizeof(REQ_PartyChangeLeaderPacket))
            {
                auto* req = reinterpret_cast<REQ_PartyChangeLeaderPacket*>(packet);
                ChangeLeader(session, req->targetPlayerID);
            }
            break;
        default:
            break;
    }
}

// 게임 종료
void PartyManager::onDisconnect(uint64_t playerID)
{
    std::lock_guard lock(lock_);
    // 파티 초대 목록 제거
    invites_.erase(playerID);

    // 파티가 있으면 파티 탈퇴
    if (Party* party = FindPartyOf(playerID))
        RemoveMember(*party, playerID);
}

void PartyManager::SyncToZone(const ClientProxySessionRef& session)
{
    std::lock_guard lock(lock_);
    // 파티가 있으면 동기화
    auto it = memberParty_.find(session->GetPlayerID());
    if (it != memberParty_.end())
        SendZoneSync(session, it->second);
}

// 파티 초대 요청
void PartyManager::Invite(const ClientProxySessionRef& inviter, uint64_t targetID)
{
    const uint64_t inviterID = inviter->GetPlayerID();
    if (inviterID == 0 || targetID == 0 || inviterID == targetID)
        return SendResult(inviter, PartyResult::InvalidTarget);

    ClientProxySessionRef target = SessionManager::GetInstance()->FindByPlayer(targetID);
    if (!target || target->GetState() != SessionState::InGame)
        return SendResult(inviter, PartyResult::TargetOffline);

    std::lock_guard<std::mutex> lock(lock_);

    // 파티 가입 중
    if (memberParty_.count(targetID))
        return SendResult(inviter, PartyResult::TargetInParty);

    if (Party* party = FindPartyOf(inviterID))
    {
        if (party->leaderID != inviterID)
            return SendResult(inviter, PartyResult::NotLeader);
        if (party->members.size() >= Game::MAX_PARTY_SIZE)
            return SendResult(inviter, PartyResult::PartyFull);
    }

    // 아직 유효한 다른 초대가 있으면 거절
    const auto now = std::chrono::steady_clock::now();
    auto& received = invites_[targetID];
    received[inviterID] = now + std::chrono::seconds(GatewayConst::PARTY_INVITE_SEC);

    RES_PartyInvitePacket pkt{};
    pkt.header.id = PKT_S_PARTY_INVITE;
    pkt.header.size = sizeof(pkt);
    pkt.inviterID = inviterID;
    strncpy_s(pkt.inviterName, inviter->GetName().c_str(), _TRUNCATE);
    target->Send(SendBufferManager::Make(&pkt, sizeof(pkt)));

    SendResult(inviter, PartyResult::InviteSent);
}

// 수락 거절
void PartyManager::Answer(const ClientProxySessionRef& target, uint64_t inviterID, bool accept)
{
    const uint64_t targetID = target->GetPlayerID();

    std::lock_guard<std::mutex> lock(lock_);

    // 파티 초대 목록 확인
    auto it = invites_.find(targetID);
    if (it == invites_.end())
        return SendResult(target, PartyResult::InviteExpired);

    // 초대 유효 시간 만료
    auto t = it->second.find(inviterID);
    const bool expired = std::chrono::steady_clock::now() >= t->second;
    invites_.erase(it);

    if (expired)
        return SendResult(target, PartyResult::InviteExpired);

    // 초대자 세션 확인
    ClientProxySessionRef inviter = SessionManager::GetInstance()->FindByPlayer(inviterID);

    // 초대 거절
    if (!accept)
    {
        if (inviter)
            SendResult(inviter, PartyResult::Declined);
        return;
    }

    if (!inviter || inviter->GetState() != SessionState::InGame)
        return SendResult(target, PartyResult::TargetOffline);
    if (memberParty_.count(targetID))
        return SendResult(target, PartyResult::AlreadyInParty);

    Party* party = FindPartyOf(inviterID);
    if (!party)
    {
        // 초대한 사람이 파티가 없으면 새로 생성
        const uint64_t partyID = nextPartyID_++;
        party = &parties_[partyID];
        party->partyID = partyID;
        party->leaderID = inviterID;
        party->members.push_back(inviterID);
        memberParty_[inviterID] = partyID;
    }
    else if (party->leaderID != inviterID)
        return SendResult(target, PartyResult::InviteExpired);

    if (party->members.size() >= Game::MAX_PARTY_SIZE)
        return SendResult(target, PartyResult::PartyFull);

    party->members.push_back(targetID);
    memberParty_[targetID] = party->partyID;

    SendZoneSync(target, party->partyID);
    BroadcastInfo(*party);
}

// 파티 탈퇴
void PartyManager::Leave(const ClientProxySessionRef& session)
{
    std::lock_guard lock(lock_);
    Party* party = FindPartyOf(session->GetPlayerID());
    if (!party)
        return SendResult(session, PartyResult::NotInParty);

    RemoveMember(*party, session->GetPlayerID());
}

void PartyManager::Kick(const ClientProxySessionRef& leader, uint64_t targetID)
{
    const uint64_t leaderID = leader->GetPlayerID();
    std::lock_guard lock(lock_);
    Party* party = FindPartyOf(leaderID);
    if (!party)
        return SendResult(leader, PartyResult::NotInParty);
    if (party->leaderID != leaderID)
        return SendResult(leader, PartyResult::NotLeader);
    if (targetID == leaderID || FindPartyOf(targetID) != party)
        return SendResult(leader, PartyResult::InvalidTarget);

    if (ClientProxySessionRef kicked = SessionManager::GetInstance()->FindByPlayer(targetID))
        SendResult(kicked, PartyResult::Kicked);

    RemoveMember(*party, targetID);
}

void PartyManager::ChangeLeader(const ClientProxySessionRef& leader, uint64_t targetID)
{
    const uint64_t leaderID = leader->GetPlayerID();

    std::lock_guard lock(lock_);
    Party* party = FindPartyOf(leaderID);
    if (!party)
        return SendResult(leader, PartyResult::NotInParty);
    if (party->leaderID != leaderID)
        return SendResult(leader, PartyResult::NotLeader);
    if (targetID == leaderID || FindPartyOf(targetID) != party)
        return SendResult(leader, PartyResult::InvalidTarget);

    party->leaderID = targetID;
    BroadcastInfo(*party);
}

Party* PartyManager::FindPartyOf(uint64_t playerID)
{
    auto m = memberParty_.find(playerID);
    if (m == memberParty_.end())
        return nullptr;

    auto p = parties_.find(m->second);

    return p != parties_.end() ? &p->second : nullptr;
}

void PartyManager::RemoveMember(Party& party, uint64_t playerID)
{
    auto& members = party.members;
    // 실제 파티에서 삭제
    members.erase(std::remove(members.begin(), members.end(), playerID), members.end());
    // 파티 멤버 리스트에서 삭제
    memberParty_.erase(playerID);

    if (ClientProxySessionRef s = SessionManager::GetInstance()->FindByPlayer(playerID))
    {
        // 클라이언트에 전달
        SendNoParty(s);
        // 존 서버에 전달
        SendZoneSync(s, 0);
    }

    // 멤버가 없다면 해산
    if (members.size() <= 0)
    {
        parties_.erase(party.partyID); // 이후 party 사용 불가
        return;
    }

    // 파티장이 탈퇴했다면 맨 앞 멤버에게 파티장 전달
    if (party.leaderID == playerID)
        party.leaderID = members.front();

    // 파티원들에게 전달
    BroadcastInfo(party);
}

// 파티 멤버들에게 전달
void PartyManager::BroadcastInfo(const Party& party)
{
    std::vector<ClientProxySessionRef> sessions;
    std::vector<PartyMemberInfo> infos;
    sessions.reserve(party.members.size());
    infos.reserve(party.members.size());

    for (uint64_t playerID : party.members)
    {
        PartyMemberInfo info{};
        info.playerID = playerID;
        if (ClientProxySessionRef s = SessionManager::GetInstance()->FindByPlayer(playerID))
        {
            info.charID = s->GetCharID();
            info.zoneID = s->GetCurrentZoneID();
            strncpy_s(info.name, s->GetName().c_str(), _TRUNCATE);
            sessions.push_back(s);
        }
        infos.push_back(info);
    }

    for (ClientProxySessionRef& s : sessions)
        SendInfo(s, &party, infos);
}

void PartyManager::SendInfo(
    const ClientProxySessionRef& session, const Party* party, const std::vector<PartyMemberInfo>& members)
{
    RES_PartyInfoPacket pkt{};
    pkt.header.id = PKT_S_PARTY_INFO;
    pkt.leaderID = party->leaderID;
    pkt.partyID = party->partyID;
    pkt.count = static_cast<uint8_t>(members.size());
    if (SendBufferRef buf = SendBufferManager::MakeList(pkt, members))
        session->Send(buf);
}

void PartyManager::SendNoParty(const ClientProxySessionRef& session)
{
    RES_PartyInfoPacket pkt{};
    pkt.header.id = PKT_S_PARTY_INFO;
    pkt.header.size = sizeof(pkt);
    session->Send(SendBufferManager::Make(&pkt, sizeof(pkt)));
}

void PartyManager::SendResult(const ClientProxySessionRef& session, PartyResult result)
{
    RES_PartyResultPacket pkt{};
    pkt.header.id = PKT_S_PARTY_RESULT;
    pkt.header.size = sizeof(pkt);
    pkt.result = result;
    session->Send(SendBufferManager::Make(&pkt, sizeof(pkt)));
}

void PartyManager::SendZoneSync(const ClientProxySessionRef& session, uint64_t partyID)
{
    PartySyncPayload payload{session->GetPlayerID(), partyID};
    ZoneManager::GetInstance()->SendInternalToZone(
        session->GetCurrentZoneID(), session->GetCurrentChannelID(), session->GetSessionID(), PKT_INTERNAL_PARTY_SYNC,
        &payload, sizeof(payload));
}
