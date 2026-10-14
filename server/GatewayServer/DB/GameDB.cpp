#include "pch.h"
#include "GameDB.h"

void GameDB::SaveCharacterProgressAsync(uint64_t playerID, uint64_t exp, float x, float y, float z, int hp, int mp, uint32_t zoneID)
{
	GameDB::Instance().PushTask([playerID, exp, x, y, z, hp, mp, zoneID](mysqlx::Session& dbSession) {
		dbSession.sql("UPDATE `character_state` SET exp = ? , posX = ? , posY = ? , posZ = ? , hp = ?, mp = ?, zoneID = ?, updateTime = NOW() WHERE playerID = ? ")
			.bind(exp, x, y, z, hp, mp, zoneID, playerID)
			.execute();
		});
}
