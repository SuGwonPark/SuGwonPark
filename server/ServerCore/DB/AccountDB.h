#pragma once
#include "ServerCore/DB/DBThreadPool.h"

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

	

private:
	AccountDB() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};

