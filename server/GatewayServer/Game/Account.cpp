#include "pch.h"
#include "Account.h"
#include "ServerCore/DB/AccountDB.h"
#include "ServerCore/DB/GameDB.h"
#include "ServerCore/Manager/SendBufferManager.h"
#include "Common/Protocol/Packets.h"
#include "Common/Protocol/PacketID.h"
#include "Manager/ZoneManager.h" 
#include "Manager/SessionManager.h"

void Account::Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt)
{
	// 현재 상태가 Connected가 아니면 무시
	if (!session->TryTransition(SessionState::Connected, SessionState::LoggingIn)) return;

	// 클라이언트가 보낸 char[]는 널 종료가 보장되지 않으니 길이를 제한해서 복사
	std::string userID(pkt->userID, strnlen(pkt->userID, sizeof(pkt->userID)));
	std::string password(pkt->password, strnlen(pkt->password, sizeof(pkt->password)));

	AccountDB::Instance().PushTask(
		[session, userID, password](mysqlx::Session& sess) {
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

			try {
				auto row = sess.sql(
					"SELECT userUID, passwordHash FROM `account` "
					"WHERE id = ? AND `remove` = 0")
					.bind(userID).execute().fetchOne();

				if (!row) { reply(false, 0, "LOGIN_FAILED"); return; }

				std::string hash = row[1].get<std::string>();
				if (crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) != 0) {
					reply(false, 0, "LOGIN_FAILED");
					return;
				}

				uint64_t userUID = row[0].get<int64_t>();
				// 같은 계정이 이미 접속 중이면: 기존 접속을 끊고, 이번 로그인은 거부
				ClientProxySessionRef existing;
				if (!SessionManager::GetInstance()->TryBindUser(userUID, session->GetSessionID(), existing)) {
					if (existing) existing->Close();     // → Zone에 접속 종료 통보 → 저장 → 바인딩 해제
					reply(false, 0, "DUPLICATE_LOGIN");  // 클라: "기존 접속을 종료했습니다. 다시 로그인해 주세요"
					return;
				}

				session->SetUserUID(userUID);
				session->SetState(SessionState::Lobby);
				reply(true, userUID, "OK");
			}
			catch (...) {
				fail("SERVER_ERROR");
				throw;
			}
		});
}

void Account::SendCharacterList(ClientProxySessionRef session) {
	if (session->GetState() != SessionState::Lobby)return;

	uint64_t userUID = session->GetUserUID();
	if (userUID == 0) return;

	GameDB::Instance().PushTask([session, userUID](mysqlx::Session& db) {

		auto result = db.sql(
			"SELECT c.playerID, c.name, cs.exp FROM `character` c JOIN `character_state` cs "
			"ON c.playerID = cs.playerID "
			"WHERE c.userUID = ? AND c.deleteTime IS NULL")
			.bind(userUID).execute();

		std::vector<CharacterSummary> list;
		for (auto row : result.fetchAll()) {
			CharacterSummary c{};
			c.playerID = row[0].get<uint64_t>();
			strncpy_s(c.name, row[1].get<std::string>().c_str(), _TRUNCATE);
			c.exp = row[2].get<uint64_t>();
			list.push_back(c);
		}

		// 메모리 공간 확보
		RES_CharacterListPacket res{};
		res.header.id = PKT_S_CHARACTER_LIST;
		res.count = static_cast<uint8_t> (list.size());

		SendBufferRef buf = SendBufferManager::MakeList(res, list);
		if (buf) session->Send(buf);
		});
}

void Account::SelectCharacter(ClientProxySessionRef session, uint64_t playerID) {
	if (!session->TryTransition(SessionState::Lobby, SessionState::Selecting))return;
	uint64_t userUID = session->GetUserUID();

	GameDB::Instance().PushTask([session, userUID, playerID](mysqlx::Session& db) {
		RES_SelectCharacterPacket res{};
		res.header.id = PKT_S_SELECT_CHARACTER;
		res.header.size = sizeof(res);

		try {
			auto row = db.sql(
				"SELECT cs.zoneID FROM `character` c "
				"JOIN `character_state` cs ON cs.playerID = c.playerID "
				"WHERE c.playerID = ? AND c.userUID = ? AND c.deleteTime IS NULL")
				.bind(playerID, userUID).execute().fetchOne();

			if (row) {
				uint32_t zoneID = row[0].get<uint32_t>();

				auto channelID = ZoneManager::GetInstance()->PickChannel(zoneID);
				if (channelID) {
					session->SetPlayerID(playerID);
					session->SetCurrentChannelID(*channelID);
					session->SetCurrentZoneID(zoneID);
					res.success = true;
					res.zoneID = zoneID;
				}
			}
		}
		catch (...) {
			session->SetState(SessionState::Lobby);
			session->Send(SendBufferManager::Make(&res, sizeof(res)));
			throw;
		}

		session->SetState(res.success ? SessionState::InGame : SessionState::Lobby);
		session->Send(SendBufferManager::Make(&res, sizeof(res)));
		});
}
