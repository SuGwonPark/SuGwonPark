#pragma once
#include "pch.h"

using Job = std::function<void()>;

class WorkStealingQueue {
public:
	// 소유자(Owner)가 최상단에 작업 삽입 (Push Top)
	void Push(Job job) {
		std::lock_guard<std::mutex> lock(lock_);
		deque_.push_back(std::move(job));
	}

	// 소유자(Owner)가 최상단에서 작업 꺼냄 (Pop Top - LIFO)
	std::optional<Job> Pop() {
		std::lock_guard<std::mutex> lock(lock_);
		if (deque_.empty()) return std::nullopt;

		Job job = std::move(deque_.back());
		deque_.pop_back();
		return job;
	}

	// 다른 유휴 스레드(Thief)가 최하단에서 작업을 훔쳐감 (Steal Bottom - FIFO)
	std::optional<Job> Steal() {
		std::lock_guard<std::mutex> lock(lock_);
		if (deque_.empty()) return std::nullopt;

		Job job = std::move(deque_.front());
		deque_.pop_front();
		return job;
	}

	bool Empty() {
		std::lock_guard<std::mutex> lock(lock_);
		return deque_.empty();
	}

private:
	std::mutex lock_;
	std::deque<Job> deque_;
};