#include "pch.h"
#include "Account.h"
#include "DB/AccountDB.h"
#include "DB/GameDB.h"
#include "Manager/SendBufferManager.h"
#include "Protocol/Packets.h"
#include "Protocol/InternalPackets.h" 

void Account::Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt)
{
	// 클라이언트가 보낸 char[]는 널 종료가 보장되지 않으니 길이를 제한해서 복사
	std::string userID(pkt->userID, strnlen(pkt->userID, sizeof(pkt->userID)));
	std::string password(pkt->password, strnlen(pkt->password, sizeof(pkt->password)));

	//AccountDB::Instance().PushTask(
	//	[session, userID, password](mysqlx::Session& sess) {
	//		auto reply = [session](bool ok, int64_t userUID, const char* msg) {
	//			RES_LoginPacket res{};
	//			res.header.id = PKT_S_LOGIN;
	//			res.header.size = sizeof(res);
	//			res.success = ok;
	//			res.userUID = 0;
	//			strcpy_s(res.message, msg);
	//			session->Send(SendBufferManager::Make(&res, sizeof(res)));
	//			};

	//		auto result = sess.getSchema("account").getTable("account")
	//			.select("id", "password_hash")
	//			.where("id = :id")
	//			.bind("id", userID)
	//			.execute();

	//		auto row = result.fetchOne();
	//		if (row) {
	//			std::string hash = row[1].get<std::string>();
	//			if (crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0) {
	//				res.success = true;
	//				res.playerID = row[0].get<uint64_t>();
	//				strcpy_s(res.message, "로그인 성공");
	//			}
	//			else {
	//				strcpy_s(res.message, "비밀번호 오류");
	//			}
	//		}
	//		else {
	//			// 계정 생성 처리 
	//		}


	//		session->Send(SendBufferManager::Make(&res, sizeof(res)));
	//	});
}

void Account::SendCharacterList(ClientProxySessionRef session) {
	//uint64_t userUID = session->GetUserUID();
	//if (userUID = 0) return;

	//GameDB::Instance().PushTask([session, userUID](mysqlx::Session& db) {
	//	RES_CharacterListPacket res{};
	//	res.header.id = PKT_S_CHARACTER_LIST;
	//	res.header.size = sizeof(res);

	//	auto result = db.sql(
	//		"SELECT playerID, name, zoneID, exp, FROM `character` "
	//		"WHERE userUID = ? AND deleteTime IS NULL")
	//		.bind(userUID).execute();

	//	for (auto row : result.fetchAll()) {
	//		if (res.count >= MAX_CHARACTERS) break;
	//		CharacterInfo& c = res.chars[res.count++];
	//	}


	//	});
}

void Account::SelectCharacter(ClientProxySessionRef session) {
}
