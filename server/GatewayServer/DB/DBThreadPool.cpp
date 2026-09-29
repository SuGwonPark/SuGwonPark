#include <pch.h>
#include "DB/DBThreadPool.h"

namespace DB {
	DBThreadPool::DBThreadPool(size_t threadCount, const std::string& connUrl)
		: connUrl_(connUrl) {
		for (size_t i = 0; i < threadCount; i++) {
			// workers_ vector에 새로운 작업용 스레드를 생성하고 WorkerLoop 함수를 실행하도록 등록
			workers_.emplace_back(&DBThreadPool::WorkerLoop, this);
		}
	}

	DBThreadPool::~DBThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			stop_ = true;
		}

		// 모든 스레드를 종료하도록 알림
		cv_.notify_all();

		// 종료 전에 큐에 남은 DB 작업이 100% 처리될 때까지 기다림
		for (std::thread& worker : workers_) {
			// 스레드가 아직 실행 중인지 확인
			if (worker.joinable()) {
				// 스레드가 끝나기 전까지 대기
				worker.join();
			}
		}
	}

	void DBThreadPool::PushTask(DBTask task) {
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			taskQueue_.push(std::move(task));
		}
		cv_.notify_one();  // 작업이 생겼을 때 하나의 스레드가 작업을 처리하도록 알림
	}

	void DBThreadPool::WorkerLoop() {
		// 스레드별 전용 DB 세션 생성 (Thread Safety 보장)
		mysqlx::Session session(connUrl_);

		while (true) {
			DBTask task;
			{
				std::unique_lock<std::mutex> lock(queueMutex_);
				cv_.wait(lock, [this]() {return stop_ || !taskQueue_.empty(); });
			}

			if (stop_ && taskQueue_.empty()) return;

			task = std::move(taskQueue_.front());
			taskQueue_.pop();

			try {
				task(session);  // DB 작업 실행
			}
			catch (const mysqlx::Error& err) {
				std::cerr << "DB ERROR: " << err.what() << std::endl;
			}
		}
	}
}
