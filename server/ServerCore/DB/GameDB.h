#pragma once
#include "ServerCore/DB/DBThreadPool.h"

class GameDB {
public:
	static GameDB& Instance() {
		static GameDB instance;
		return instance;
	}

	// DB 연결
	bool Init(size_t threadCount, const std::string& connUrl) {
		pool_ = std::make_unique<DB::DBThreadPool>(threadCount, connUrl);
		return true;
	}

	void PushTask(DB::DBTask task) {
		if (pool_) {
			pool_->PushTask(std::move(task));
		}
	}

	void PushTask(uint64_t key, DB::DBTask task) {
		if (pool_) {
			pool_->PushTask(key, std::move(task));
		}
	}

	void SaveCharacterProgressAsync(uint64_t playerID, uint64_t exp, float x, float y, float z, int hp, int mp, uint32_t zoneID, std::function<void(bool)> onDone = nullptr);

private:
	GameDB() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};