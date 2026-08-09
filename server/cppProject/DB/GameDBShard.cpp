#include "pch.h"
#include "GameDBShard.h"

void GameDBShard::SaveCharacterProgressAsync(uint64_t pid, int exp, int level)
{
	// 비동기 캐릭터 진행 상황 저장
	GameDBShard::Instance().PushTask(pid, [pid, exp, level](mysqlx::Session& dbSession) {
		dbSession.sql("UPDATE characters SET exp = ?, level = ? WHERE id = ?")
			.bind(exp, level, pid)
			.execute();
		});
}
