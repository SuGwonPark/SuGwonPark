#pragma once
#include "ServerCore/DB/DBTypes.h"

namespace DB {
	class DBThreadPool {
	public:
		DBThreadPool(size_t threadCount, const std::string& connUrl);
		~DBThreadPool();

		// 순서 무관 작업 (로그인, 목록 조회 등) → 워커를 돌아가며 배정
		void PushTask(DBTask task);
		// 같은 key는 항상 같은 워커 → key 단위로 실행 순서 보장
		void PushTask(uint64_t key, DBTask task);

	private:
		struct Worker {
			std::thread thread;
			std::queue<DBTask> queue;
			std::mutex mutex;
			std::condition_variable cv;
			bool stop = false;
		};

		// Worker Queue에 데이터 삽입
		void Enqueue(Worker& w, DBTask task);
		// Worker의 Qeue에 Task를 실행
		void WorkerLoop(Worker& w);

		// 세션 연결
		bool Connect(std::unique_ptr<mysqlx::Session>& session);
		// 세션 상태 확인
		static bool IsAlive(mysqlx::Session& session);


	private:
		// mutex/condition_variable은 복사·이동이 안 되므로 unique_ptr로 보관
		std::vector<std::unique_ptr<Worker>> workers_;
		std::atomic<size_t> nextWorker_{ 0 };
		std::string connUrl_;


		static constexpr auto IDLE_PING_INTERVAL = std::chrono::seconds(30);
		static constexpr auto RECONNECT_DELAY = std::chrono::seconds(1);
	};
}