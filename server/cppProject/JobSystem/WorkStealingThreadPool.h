#pragma once
#include "pch.h"
#include "WorkStealingQueue.h"


class WorkStealingThreadPool {
public:
	explicit WorkStealingThreadPool(uint32_t threadCount = std::thread::hardware_concurrency())
		: threadCount_(threadCount), isRunning_(true), queues_(threadCount) {

		for (uint32_t i = 0; i < threadCount_; ++i) {
			queues_[i] = std::make_unique<WorkStealingQueue>();
		}

		for (uint32_t i = 0; i < threadCount_; ++i) {
			workers_.emplace_back(&WorkStealingThreadPool::WorkerLoop, this, i);
		}
	}

	~WorkStealingThreadPool() {
		Stop();
	}

	// 작업을 특정 스레드 로컬 큐에 등록 (인덱스 미지정 시 랜덤 할당)
	void Post(Job job, int32_t targetThreadIndex = -1) {
		if (targetThreadIndex >= 0 && targetThreadIndex < static_cast<int32_t>(threadCount_)) {
			queues_[targetThreadIndex]->Push(std::move(job));
		}
		else {
			static thread_local std::mt19937 rng(std::random_device{}());
			std::uniform_int_distribution<uint32_t> dist(0, threadCount_ - 1);
			queues_[dist(rng)]->Push(std::move(job));
		}
	}

	void Stop() {
		if (!isRunning_.exchange(false)) return;
		for (auto& t : workers_) {
			if (t.joinable()) t.join();
		}
	}

private:
	void WorkerLoop(uint32_t threadId) {
		while (isRunning_.load()) {
			std::optional<Job> job = queues_[threadId]->Pop();

			// 1. 자신의 로컬 큐에 작업이 있으면 즉시 실행
			if (job.has_value()) {
				(*job)();
				continue;
			}

			// 2. 내 큐가 비었으면 다른 스레드의 큐를 탐색하여 Steal 시도
			bool stoleJob = false;
			for (uint32_t offset = 1; offset < threadCount_; ++offset) {
				uint32_t victimId = (threadId + offset) % threadCount_;
				job = queues_[victimId]->Steal();

				if (job.has_value()) {
					(*job)();
					stoleJob = true;
					break;
				}
			}

			// 3. 훔칠 일감도 전혀 없으면 양보(Yield)
			if (!stoleJob) {
				std::this_thread::yield();
			}
		}
	}

private:
	uint32_t threadCount_;
	std::atomic<bool> isRunning_;
	std::vector<std::unique_ptr<WorkStealingQueue>> queues_;
	std::vector<std::thread> workers_;
};