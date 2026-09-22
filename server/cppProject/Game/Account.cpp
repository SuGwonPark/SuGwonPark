#include "pch.h"
#include "Account.h"
#include "DB/GameDB.h"
#include "Manager/SendBufferManager.h"
#include <sodium.h>

void Account::Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt)
{
	std::string userId(pkt->userId);
	std::string password(pkt->password);

	GameDB::Instance().PushTask(
		[session, userId, password](mysqlx::Session& sess) {
			RES_LoginPacket res{};
			res.header.id = PKT_S_LOGIN;
			res.header.size = sizeof(res);
			res.success = false;
			res.playerId = -1;

			auto result = sess.getSchema("gamedb").getTable("accounts")
				.select("id", "password_hash")
				.where("user_id = :uid")
				.bind("uid", userId)
				.execute();

			auto row = result.fetchOne();
			if (row) {
				std::string hash = row[1].get<std::string>();
				if (crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0) {
					res.success = true;
					res.playerId = row[0].get<int32_t>();
					strcpy_s(res.message, "로그인 성공");
				}
				else {
					strcpy_s(res.message, "비밀번호 오류");
				}
			}
			else {
				// 계정 생성 처리 
			}


			session->Send(SendBufferManager::Make(&res, sizeof(res)));
		});
}
