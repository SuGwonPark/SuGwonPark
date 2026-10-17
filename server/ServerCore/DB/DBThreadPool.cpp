#include <pch.h>
#include "ServerCore/DB/DBThreadPool.h"

namespace DB {
	DBThreadPool::DBThreadPool(size_t threadCount, const std::string& connUrl)
		: connUrl_(connUrl) {
		if (threadCount == 0) threadCount = 1;

		// 메인 스레드에서 한 번 접속해 본다.
		// 실패하면 예외가 Init() → main()의 catch까지 올라가서 서버가 정상적으로 종료된다
		{
			mysqlx::Session probe(connUrl_);
			probe.close();
		}

		for (size_t i = 0; i < threadCount; ++i)
			workers_.push_back(std::make_unique<Worker>());

		// Worker 객체를 전부 만든 뒤에 스레드를 시작한다
		for (auto& w : workers_)
			w->thread = std::thread(&DBThreadPool::WorkerLoop, this, std::ref(*w));
	}


	DBThreadPool::~DBThreadPool() {
		for (auto& w : workers_) {
			{
				std::lock_guard<std::mutex> lock(w->mutex);
				w->stop = true;
			}
			w->cv.notify_one();
		}
		// 각 워커는 자기 큐를 다 비운 뒤 종료한다
		for (auto& w : workers_) {
			if (w->thread.joinable()) w->thread.join();
		}
	}

	void DBThreadPool::PushTask(DBTask task) {
		size_t idx = nextWorker_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
		Enqueue(*workers_[idx], std::move(task));
	}

	void DBThreadPool::PushTask(uint64_t key, DBTask task) {
		Enqueue(*workers_[key % workers_.size()], std::move(task));
	}

	void DBThreadPool::Enqueue(Worker& w, DBTask task) {
		{
			std::lock_guard<std::mutex> lock(w.mutex);
			w.queue.push(std::move(task));
		}
		w.cv.notify_one();
	}

	void DBThreadPool::WorkerLoop(Worker& w) {
		std::unique_ptr<mysqlx::Session> session;     // 처음엔 비어 있고, 첫 작업 때 접속
		auto lastUsed = std::chrono::steady_clock::now();

		while (true) {
			DBTask task;
			{
				std::unique_lock<std::mutex> lock(w.mutex);
				w.cv.wait(lock, [&w]() { return w.stop || !w.queue.empty(); });
				if (w.stop && w.queue.empty()) return;
				task = std::move(w.queue.front());
				w.queue.pop();
			}

			// 오래 놀던 세션은 MySQL이 끊었을 수 있으니 먼저 확인
			if (session && std::chrono::steady_clock::now() - lastUsed > IDLE_PING_INTERVAL
				&& !IsAlive(*session)) {
				session.reset();
			}

			// 연결될 때까지 재시도. 작업은 버리지 않고 DB가 돌아오면 순서대로 처리한다
			while (!session && !Connect(session)) {
				{
					std::lock_guard<std::mutex> lock(w.mutex);
					if (w.stop) {
						std::cerr << "[DB] 종료 중 연결 불가, 남은 작업 폐기" << std::endl;
						return;
					}
				}
				std::this_thread::sleep_for(RECONNECT_DELAY);
			}

			try {
				task(*session);
			}
			catch (const mysqlx::Error& err) {
				std::cerr << "DB ERROR: " << err.what() << std::endl;
				// SQL 문법 오류인지 연결 끊김인지 구분: 핑이 안 되면 다음 작업 전에 재연결
				if (!IsAlive(*session)) session.reset();
			}
			catch (const std::exception& err) {
				std::cerr << "DB TASK ERROR: " << err.what() << std::endl;
			}
			lastUsed = std::chrono::steady_clock::now();
		}
	}

	bool DBThreadPool::Connect(std::unique_ptr<mysqlx::Session>& session) {
		try {
			// 세션 연결
			session = std::make_unique<mysqlx::Session>(connUrl_);
			return true;
		}
		catch (const mysqlx::Error& e) {
			std::cerr << "[DB] 연결 실패: " << e.what() << std::endl;
			session.reset();
			return false;
		}
	}

	bool DBThreadPool::IsAlive(mysqlx::Session& session) {
		try {
			session.sql("SELECT 1").execute();
			return true;
		}
		catch (const mysqlx::Error&) {
			return false;
		}
	}
}
