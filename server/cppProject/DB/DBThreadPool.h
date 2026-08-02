#pragma once
#include "DB/DBTypes.h"

namespace DB {
	class DBThreadPool {
	public:
		DBThreadPool(size_t threadCount, const std::string& connUrl);
		~DBThreadPool();

		// Game Thread에서 비동기 DB 작업을 큐에 밀어넣을 때 호출 (Non-Blocking)
		void PushTask(DBTask task);

	private:
		void WorkerLoop();

	private:
		std::vector<std::thread> workers_;
		std::queue<DBTask> taskQueue_;
		std::mutex queueMutex_;
		std::condition_variable cv_;
		bool stop_ = false;
		std::string connUrl_;

	};
}