#pragma once
#include "DB/DBThreadPool.h"

class DBManager {
public:
	static DBManager& Instance() {
		static DBManager instance;
		return instance;
	}

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
	DBManager() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};