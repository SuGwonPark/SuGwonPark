#pragma once
#include "DB/DBThreadPool.h"

class GameDBShard {
public:
	static GameDBShard& Instance() {
		static GameDBShard  instance;
		return instance;
	}

	// DB ¿¬°á
	bool Init(size_t threadCount, const std::vector<std::string>& connUrls) {
		for (const std::string& url : connUrls)
		{
			shardPools_.push_back(std::make_unique<DB::DBThreadPool>(threadCount, url));
		}

		return shardPools_.size() == connUrls.size();
	}

	void PushTask(uint64_t playerId, DB::DBTask task) {
		if (shardPools_.empty())  return;

		size_t shardIdx = static_cast<size_t>(playerId % shardPools_.size());
		shardPools_[shardIdx]->PushTask(std::move(task));
	}

	void SaveCharacterProgressAsync(uint64_t pid, int exp, int level);

private:
	GameDBShard() = default;
	std::vector<std::unique_ptr<DB::DBThreadPool>> shardPools_;
};