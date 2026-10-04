#pragma once
#include "DB/DBThreadPool.h"

class AccountDB
{
public:
	static AccountDB& Instance() {
		static AccountDB instance;
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

	void SaveCharacterProgressAsync(uint64_t userUID, uint64_t exp, float x, float y, float z, int hp);

private:
	AccountDB() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};

