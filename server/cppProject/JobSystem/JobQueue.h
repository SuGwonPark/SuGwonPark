#pragma once
#include "pch.h"

using Job = std::function<void()>;

class JobQueue : public std::enable_shared_from_this<JobQueue> {
public:
	// 작업을 큐에 삽입 (FiFO)
	void Push(Job job) {
		std::lock_guard<std::mutex> lock(lock_);
		jobs_.push_back(std::move(job));
	}

	// 템플릿 헬퍼: 클래스 멤버 함수를 직관적으로 Push
	template<typename T, typename Ret, typename... Args>
	void Push(Ret(T::* memFunc)(Args...), std::shared_ptr<T> owner, Args... args) {
		Push([owner, memFunc, args...]() {
			(owner.get()->*memFunc)(args...);
			});
	}

	// 게임 로직 스레드에서 주기적으로 호출하여 쌓인 Job 일괄 실행
	void Execute() {
		// 동일한 JobQueue를 여러 스레드가 동시에 실행하지 못하도록 방어
		if (isExecuting_.exchange(true) == true) {
			return;
		}

		while (true) {
			std::vector<Job> executionList;
			{
				std::lock_guard<std::mutex> lock(lock_);
				if (jobs_.empty()) {
					isExecuting_.store(false);
					return;
				}
				// 락을 쥔 시간을 최소화하기 위해 내부 버퍼 포인터만 빠르게 스왑
				executionList.swap(jobs_);
			}

			// 락이 완전히 풀린 상태에서 순서대로(FIFO) 실행
			for (auto& job : executionList) {
				job();
			}
		}
	}

	void Clear() {
		std::lock_guard<std::mutex> lock(lock_);
		jobs_.clear();
	}

	int32_t GetJobCount() {
		std::lock_guard<std::mutex> lock(lock_);
		return static_cast<int32_t>(jobs_.size());
	}


private:
	std::mutex lock_;
	std::vector<Job> jobs_;
	std::atomic<bool> isExecuting_{ false };
};

using JobQueueRef = std::shared_ptr<JobQueue>;