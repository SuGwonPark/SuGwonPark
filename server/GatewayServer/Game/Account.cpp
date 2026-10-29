#include "pch.h"
#include "Account.h"
#include "ServerCore/DB/AccountDB.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "ServerCore/Protocol/Packets.h"
#include "ServerCore/Protocol/PacketID.h"
#include "Manager/ZoneManager.h"
#include "Manager/SessionManager.h"

namespace
{
// UTF-8 한 글자를 읽어 code에 담고 다음 위치를 돌려준다. 잘못된 바이트면 npos
size_t NextCodePoint(const std::string& s, size_t i, char32_t& code)
{
    const unsigned char c = static_cast<unsigned char>(s[i]);
    int len = 0;
    if (c < 0x80)
    {
        code = c;
        return i + 1;
    }
    else if ((c & 0xE0) == 0xC0)
    {
        code = c & 0x1F;
        len = 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        code = c & 0x0F;
        len = 3;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        code = c & 0x07;
        len = 4;
    }
    else
        return std::string::npos;

    if (i + len > s.size())
        return std::string::npos;
    for (int k = 1; k < len; ++k)
    {
        const unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80)
            return std::string::npos;
        code = (code << 6) | (cc & 0x3F);
    }
    return i + len;
}

// 허용 글자: 숫자, 영문 대소문자, 한글 완성형(가~힣)
bool IsAllowedChar(char32_t c)
{
    return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
           (c >= 0xAC00 && c <= 0xD7A3);
}

// 2~12 글자 (바이트가 아니라 글자 수)
bool IsValidCharacterName(const std::string& name)
{
    size_t count = 0;
    for (size_t i = 0; i < name.size();)
    {
        char32_t code = 0;
        const size_t next = NextCodePoint(name, i, code);
        if (next == std::string::npos || !IsAllowedChar(code))
            return false;
        i = next;
        ++count;
    }
    return count >= 2 && count <= 12;
}
} // namespace

void Account::CreateAccount(const std::string& userID, const std::string& password, CreateAccountCallback cb)
{
    // 길이 검사 (REQ_LoginPacket의 char[32]에 맞춤)
    if (userID.empty() || userID.size() >= 32 || password.empty() || password.size() >= 32)
    {
        if (cb)
            cb(false, "INVALID_INPUT");
        return;
    }

    // 해시 계산은 느리므로(의도된 것) DB 스레드 안에서 한다
    AccountDB::Instance().PushTask([userID, password, cb](mysqlx::Session& sess) {
        try
        {
            char hash[crypto_pwhash_STRBYTES];
            if (crypto_pwhash_str(
                    hash, password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                    crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
            {
                if (cb)
                    cb(false, "HASH_FAILED");
                return;
            }

            // 중복 확인 (삭제된 계정의 id도 UNIQUE에 걸리므로 remove 조건 없이)
            if (sess.sql("SELECT 1 FROM `account` WHERE id = ?").bind(userID).execute().fetchOne())
            {
                if (cb)
                    cb(false, "DUPLICATE_ID");
                return;
            }

            sess.sql("INSERT INTO `account` (id, passwordHash) VALUES (?, ?)")
                .bind(userID, std::string(hash))
                .execute();

            if (cb)
                cb(true, "OK");
        }
        catch (const mysqlx::Error& e)
        {
            // 확인과 INSERT 사이에 같은 id가 먼저 들어온 경우
            if (std::string(e.what()).find("Duplicate entry") != std::string::npos)
            {
                if (cb)
                    cb(false, "DUPLICATE_ID");
                return;
            }
            if (cb)
                cb(false, "SERVER_ERROR");
            throw;
        }
    });
}

void Account::Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt)
{
    // 현재 상태가 Connected가 아니면 무시
    if (!session->TryTransition(SessionState::Connected, SessionState::LoggingIn))
        return;

    // 클라이언트가 보낸 char[]는 널 종료가 보장되지 않으니 길이를 제한해서 복사
    std::string userID(pkt->userID, strnlen(pkt->userID, sizeof(pkt->userID)));
    std::string password(pkt->password, strnlen(pkt->password, sizeof(pkt->password)));

    AccountDB::Instance().PushTask([session, userID, password](mysqlx::Session& sess) {
        auto reply = [session](bool ok, int64_t userUID, const char* msg) {
            RES_LoginPacket res{};
            res.header.id = PKT_S_LOGIN;
            res.header.size = sizeof(res);
            res.success = ok;
            res.userUID = userUID;
            strncpy_s(res.message, msg, _TRUNCATE);
            session->Send(SendBufferManager::Make(&res, sizeof(res)));
        };

        auto fail = [&](const char* msg) {
            session->SetState(SessionState::Connected); // 실패시 처음부터
            reply(false, 0, msg);
        };

        try
        {
            auto row = sess.sql(
                               "SELECT userUID, passwordHash FROM `account` "
                               "WHERE id = ? AND `remove` = 0")
                           .bind(userID)
                           .execute()
                           .fetchOne();

            if (!row)
            {
                fail("LOGIN_FAILED");
                return;
            }

            std::string hash = row[1].get<std::string>();
            if (crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) != 0)
            {
                fail("LOGIN_PASSWORD_FAILED");
                return;
            }

            uint64_t userUID = row[0].get<int64_t>();
            // 같은 계정이 이미 접속 중이면: 기존 접속을 끊고, 이번 로그인은 거부
            ClientProxySessionRef existing;
            if (!SessionManager::GetInstance()->TryBindUser(userUID, session->GetSessionID(), existing))
            {
                if (existing)
                    existing->Close();   // → 기존 접속 유저 Zone에 접속 종료 통보 → 저장 → 바인딩 해제
                fail("DUPLICATE_LOGIN"); // 현재 접속 유저도 1회 거부
                return;
            }

            session->SetUserUID(userUID);
            session->SetState(SessionState::Lobby);
            reply(true, userUID, "OK");
        }
        catch (...)
        {
            fail("SERVER_ERROR");
            throw;
        }
    });
}

// 캐릭터 리스트
void Account::SendCharacterList(ClientProxySessionRef session)
{
    if (session->GetState() != SessionState::Lobby)
        return;

    uint64_t userUID = session->GetUserUID();
    if (userUID == 0)
        return;

    GameDB::Instance().PushTask([session, userUID](mysqlx::Session& db) {
        auto result = db.sql(
                            "SELECT c.playerID, c.name, cs.classType, cs.exp FROM `character` c JOIN `character_state` cs "
                            "ON c.playerID = cs.playerID "
                            "WHERE c.userUID = ? AND c.deleteTime IS NULL")
                          .bind(userUID)
                          .execute();

        std::vector<CharacterSummary> list;
        for (auto row : result.fetchAll())
        {
            CharacterSummary c{};
            c.playerID = row[0].get<uint64_t>();
            strncpy_s(c.name, row[1].get<std::string>().c_str(), _TRUNCATE);
            c.classType = static_cast<uint8_t>(row[2].get<uint32_t>());
            c.exp = row[3].get<uint64_t>();
            list.push_back(c);
        }

        // 메모리 공간 확보
        RES_CharacterListPacket res{};
        res.header.id = PKT_S_CHARACTER_LIST;
        res.count = static_cast<uint8_t>(list.size());

        SendBufferRef buf = SendBufferManager::MakeList(res, list);
        if (buf)
            session->Send(buf);
    });
}

// 캐릭터 선택
void Account::SelectCharacter(ClientProxySessionRef session, uint64_t playerID)
{
    if (!session->TryTransition(SessionState::Lobby, SessionState::Selecting))
        return;
    uint64_t userUID = session->GetUserUID();

    GameDB::Instance().PushTask([session, userUID, playerID](mysqlx::Session& db) {
        RES_SelectCharacterPacket res{};
        res.header.id = PKT_S_SELECT_CHARACTER;
        res.header.size = sizeof(res);

        try
        {
            auto row = db.sql(
                             "SELECT cs.zoneID FROM `character` c "
                             "JOIN `character_state` cs ON cs.playerID = c.playerID "
                             "WHERE c.playerID = ? AND c.userUID = ? AND c.deleteTime IS NULL")
                           .bind(playerID, userUID)
                           .execute()
                           .fetchOne();

            if (row)
            {
                uint32_t zoneID = row[0].get<uint32_t>();

                auto channelID = ZoneManager::GetInstance()->PickChannel(zoneID);
                if (channelID)
                {
                    session->SetPlayerID(playerID);
                    session->SetCurrentChannelID(*channelID);
                    session->SetCurrentZoneID(zoneID);
                    res.success = true;
                    res.zoneID = zoneID;
                }
            }
        }
        catch (...)
        {
            session->SetState(SessionState::Lobby);
            session->Send(SendBufferManager::Make(&res, sizeof(res)));
            throw;
        }

        session->SetState(res.success ? SessionState::InGame : SessionState::Lobby);
        session->Send(SendBufferManager::Make(&res, sizeof(res)));
    });
}

void Account::CreateCharacter(ClientProxySessionRef session, const REQ_CreateCharacterPacket* pkt)
{
    if (!session->TryTransition(SessionState::Lobby, SessionState::Creating))
        return;

    const uint64_t userUID = session->GetUserUID();
    std::string name(pkt->name, strnlen(pkt->name, sizeof(pkt->name)));

    // 성공/실패 모두 로비로 돌려놓고 응답
    auto reply = [session](CreateCharacterResult result, const CharacterSummary* created) {
        RES_CreateCharacterPacket res{};
        res.header.id = PKT_S_CREATE_CHARACTER;
        res.header.size = sizeof(res);
        res.result = result;
        if (created)
            res.character = *created;

        session->SetState(SessionState::Lobby);
        session->Send(SendBufferManager::Make(&res, sizeof(res)));
    };

    if (!IsValidCharacterName(name))
        return reply(CreateCharacterResult::InvalidName, nullptr);

    GameDB::Instance().PushTask(userUID, [reply, userUID, name](mysqlx::Session& db) {
        try
        {
            // 1) 슬롯 수
            auto count = db.sql("SELECT COUNT(*) FROM `character` WHERE userUID = ? AND deleteTime IS NULL")
                             .bind(userUID)
                             .execute()
                             .fetchOne();
            if (count[0].get<int64_t>() >= Game::MAX_CHARACTER_SLOT)
                return reply(CreateCharacterResult::SlotFull, nullptr);

            // 2) 이름 중복 (삭제된 캐릭터 이름도 UNIQUE에 걸리므로 deleteTime 조건 없이)
            if (db.sql("SELECT 1 FROM `character` WHERE name = ?").bind(name).execute().fetchOne())
                return reply(CreateCharacterResult::DuplicateName, nullptr);

            // 3) 두 테이블을 한 트랜잭션으로 (중간에 실패하면 둘 다 취소)
            db.startTransaction();
            try
            {
                auto inserted =
                    db.sql("INSERT INTO `character` (userUID, name) VALUES (?, ?)").bind(userUID, name).execute();
                const uint64_t playerID = inserted.getAutoIncrementValue();

                db.sql("INSERT INTO `character_state` (playerID, classType, zoneID, posX, posY, posZ) VALUES (?, ?, ?, ?, ?, ?)")
                    .bind(
                        playerID, 1, GatewayConst::START_ZONE_ID, GatewayConst::START_X, GatewayConst::START_Y,
                        GatewayConst::START_Z)
                    .execute();

                db.commit();

                CharacterSummary created{};
                created.playerID = playerID;
                created.exp = 0;
                strncpy_s(created.name, name.c_str(), _TRUNCATE);
                reply(CreateCharacterResult::Success, &created);
            }
            catch (...)
            {
                db.rollback();
                throw;
            }
        }
        catch (const mysqlx::Error& e)
        {
            // 2)와 3) 사이에 다른 계정이 같은 이름을 먼저 만든 경우: UNIQUE 위반
            if (std::string(e.what()).find("Duplicate entry") != std::string::npos)
                return reply(CreateCharacterResult::DuplicateName, nullptr);

            reply(CreateCharacterResult::ServerError, nullptr);
            throw; // 연결 문제일 수 있으니 DB 풀에 알림 (재연결 판단)
        }
        catch (...)
        {
            reply(CreateCharacterResult::ServerError, nullptr);
            throw;
        }
    });
}