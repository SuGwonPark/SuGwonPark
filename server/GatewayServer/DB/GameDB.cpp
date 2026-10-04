#include "pch.h"
#include "GameDB.h"

void GameDB::SaveCharacterProgressAsync(uint64_t playerID, uint64_t exp, float x, float y, float z, int hp)
{
	GameDB::Instance().PushTask([playerID, exp, x, y, z, hp](mysqlx::Session& dbSession) {
		dbSession.sql("UPDATE character SET exp = ? , x = ? , y = ? , z = ? , hp = ? WHERE playerID = ? ")
			.bind(exp, x, y, z, hp, playerID)
			.execute();
		});
}
