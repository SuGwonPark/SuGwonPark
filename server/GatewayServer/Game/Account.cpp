#include "pch.h"
#include "Account.h"
#include "DB/AccountDB.h"
#include "DB/GameDB.h"
#include "Manager/SendBufferManager.h"
#include "Protocol/Packets.h"
#include "Protocol/PacketID.h"

void Account::Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt)
{
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
			session->SetUserUID(userUID);
			reply(true, userUID, "OK");
		});
}

void Account::SendCharacterList(ClientProxySessionRef session) {
	uint64_t userUID = session->GetUserUID();
	if (userUID == 0) return;

	GameDB::Instance().PushTask([session, userUID](mysqlx::Session& db) {

		auto result = db.sql(
			"SELECT c.playerID, c.name, cs.exp FROM `character` c JOIN `character_state` cs "
			"ON c.playerID = cs.playerID "
			"WHERE c.userUID = ? AND c.deleteTime IS NULL "
			"ORDER BY cs.slot DESC LIMIT 6")
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
	uint64_t userUID = session->GetUserUID();
	if (userUID == 0) return;
	if (session->GetPlayerID() != 0) return;

	GameDB::Instance().PushTask([session, userUID, playerID](mysqlx::Session& db) {
		RES_SelectCharacterPacket res{};
		res.header.id = PKT_S_SELECT_CHARACTER;
		res.header.size = sizeof(res);

		auto row = db.sql(
			"SELECT cs.zoneID FROM `character` c "
			"JOIN `character_state` cs ON cs.playerID = c.playerID "
			"WHERE c.playerID = ? AND c.userUID = ? AND c.deleteTime IS NULL")
			.bind(playerID, userUID).execute().fetchOne();

		if (row) {
			uint32_t zoneID = row[0].get<uint32_t>();
			session->SetPlayerID(playerID);
			session->SetCurrentZoneID(zoneID);
			res.success = true;
			res.zoneID = zoneID;
		}
		session->Send(SendBufferManager::Make(&res, sizeof(res)));
		});
}
