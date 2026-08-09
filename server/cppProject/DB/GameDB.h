#pragma once
#include "DB/DBThreadPool.h"

class GameDB {
public:
	static GameDB& Instance() {
		static GameDB instance;
		return instance;
	}

	// DB ¿¬°á
	bool Init(size_t threadCount, const std::string& connUrl) {
		pool_ = std::make_unique<DB::DBThreadPool>(threadCount, connUrl);
		return true;
	}

	void PushTask(DB::DBTask task) {
		if (pool_) {
			pool_->PushTask(std::move(task));
		}
	}

private:
	GameDB() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};