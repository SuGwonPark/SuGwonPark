#include <pch.h>
#include "DB/DBThreadPool.h"

namespace DB {
	DBThreadPool::DBThreadPool(size_t threadCount, const std::string& connUrl)
		: connUrl_(connUrl) {
		for (size_t i = 0; i < threadCount; i++) {
			// workers_ vector에 새로운 작업자 스레드를 생성하고 WorkerLoop 함수를 실행하도록 설정
			workers_.emplace_back(&DBThreadPool::WorkerLoop, this);
		}
	}

	DBThreadPool::~DBThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			stop_ = true;
		}

		// 모든 쓰레드를 깨워서 종료하도록 알림
		cv_.notify_all();

		// 서버 종료 시 큐에 남은 DB 작업이 100% 처리될 때까지 기다림
		for (std::thread& workers_ : workers_) {
			// 쓰레드가 실행 가능한 상태
			if (workers_.joinable()) {
				// 쓰레드가 종료될 때까지 대기
				workers_.join();
			}
		}
	}

	void DBThreadPool::PushTask(DBTask task) {
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			taskQueue_.push(std::move(task));
		}
		cv_.notify_one();  // 작업자 스레드 중 하나를 깨워서 작업을 처리하도록 알림
	}

	void DBThreadPool::WorkerLoop() {
		// 스레드별 독립 DB 세션 생성 (Thread Safety 보장)
		mysqlx::Session session(connUrl_);
		session.sql("USE gamedb").execute();

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
				task(session);  // DB 작업 수행
			}
			catch (const mysqlx::Error& err) {
				std::cerr << "DB ERROR: " << err.what() << std::endl;
			}
		}
	}
}