#pragma once
#include "pch.h"
#include "WorkStealingQueue.h"

class WorkStealingThreadPool {
public:
	explicit WorkStealingThreadPool(uint32_t threadCount = std::thread::hardware_concurrency())
		: threadCount_(threadCount == 0 ? 1 : threadCount)      // 0 반환 대비
		, queues_(threadCount_) {

		for (uint32_t i = 0; i < threadCount_; ++i)
			queues_[i] = std::make_unique<WorkStealingQueue>();

		for (uint32_t i = 0; i < threadCount_; ++i)
			workers_.emplace_back(&WorkStealingThreadPool::WorkerLoop, this, i);
	}

	~WorkStealingThreadPool() { Stop(); }

	void Post(Job job, int32_t targetThreadIndex = -1) {
		uint32_t idx;
		if (targetThreadIndex >= 0 && targetThreadIndex < static_cast<int32_t>(threadCount_)) {
			idx = static_cast<uint32_t>(targetThreadIndex);
		}
		else {
			static thread_local std::mt19937 rng(std::random_device{}());
			std::uniform_int_distribution<uint32_t> dist(0, threadCount_ - 1);
			idx = dist(rng);
		}
		queues_[idx]->Push(std::move(job));
		pending_.fetch_add(1);

		// 자고 있는 워커가 있을 때만 깨운다 (없으면 락도 안 잡음)
		if (sleepers_.load() > 0) {
			std::lock_guard<std::mutex> lk(sleepMutex_);
			sleepCv_.notify_one();
		}
	}

	void Stop() {
		if (!isRunning_.exchange(false)) return;
		{
			std::lock_guard<std::mutex> lk(sleepMutex_);   // 자려던 워커와의 경합 방지
		}
		sleepCv_.notify_all();
		for (auto& t : workers_) {
			if (t.joinable()) t.join();
		}
	}

private:
	static constexpr uint32_t SPIN_BEFORE_SLEEP = 64;

	std::optional<Job> TryGetJob(uint32_t threadId) {
		if (auto job = queues_[threadId]->Pop()) return job;

		for (uint32_t offset = 1; offset < threadCount_; ++offset) {
			uint32_t victimId = (threadId + offset) % threadCount_;
			if (auto job = queues_[victimId]->Steal()) return job;
		}
		return std::nullopt;
	}

	void WorkerLoop(uint32_t threadId) {
		uint32_t idleSpins = 0;

		while (isRunning_.load()) {
			if (auto job = TryGetJob(threadId)) {
				pending_.fetch_sub(1);
				(*job)();
				idleSpins = 0;
				continue;
			}

			// 1단계: 잠깐은 바쁘게 기다린다 (곧 일이 올 때 지연을 줄임)
			if (++idleSpins < SPIN_BEFORE_SLEEP) {
				std::this_thread::yield();
				continue;
			}

			// 2단계: 그래도 없으면 잠든다. Post나 Stop이 깨울 때까지 CPU 0%
			std::unique_lock<std::mutex> lk(sleepMutex_);
			sleepers_.fetch_add(1);
			sleepCv_.wait(lk, [this] { return pending_.load() > 0 || !isRunning_.load(); });
			sleepers_.fetch_sub(1);
			idleSpins = 0;
		}
	}

private:
	uint32_t threadCount_;
	std::atomic<bool> isRunning_{ true };
	std::vector<std::unique_ptr<WorkStealingQueue>> queues_;
	std::vector<std::thread> workers_;

	std::atomic<int64_t>  pending_{ 0 };    // 모든 큐에 쌓인 작업 수
	std::atomic<uint32_t> sleepers_{ 0 };   // 잠든 워커 수
	std::mutex sleepMutex_;
	std::condition_variable sleepCv_;
};