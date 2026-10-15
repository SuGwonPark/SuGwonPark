#include "pch.h"
#include "GameDB.h"

void GameDB::SaveCharacterProgressAsync(uint64_t playerID, uint64_t exp, float x, float y, float z, int hp, int mp, uint32_t zoneID, std::function<void(bool)> onDone) {
	GameDB::Instance().PushTask([playerID, exp, x, y, z, hp, mp, zoneID, onDone = std::move(onDone)](mysqlx::Session& dbSession) {
		bool ok = false;
		try {
			dbSession.sql("UPDATE `character_state` SET exp = ? , posX = ? , posY = ? , posZ = ? , hp = ?, mp = ?, zoneID = ?, updateTime = NOW() WHERE playerID = ? ")
				.bind(exp, x, y, z, hp, mp, zoneID, playerID)
				.execute();
			ok = true;
		}
		catch (const mysqlx::Error& e) {
			std::cerr << "[SaveCharacterProgress] " << e.what() << std::endl;
		}
		if (onDone) onDone(ok);
		});
}
