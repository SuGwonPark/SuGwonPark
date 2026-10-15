#pragma once
#include "pch.h"
#include "JobSystem/JobQueue.h"
#include "JobSystem/WorkStealingThreadPool.h"

// 프로세스 전체가 공유하는 워커 풀. inline 함수의 static 지역변수라 프로그램 전체에서 한 개만 생긴다.
inline WorkStealingThreadPool& GlobalWorkerPool() {
	static WorkStealingThreadPool pool;
	return pool;
}

// "한 번에 한 스레드만 내 상태를 만진다"를 보장해야 하는 객체(Zone, 이후 Party 등)가 상속한다.
// 사용법: class Zone : public JobSerializer<Zone>
template<typename T>
class JobSerializer : public std::enable_shared_from_this<T> {
public:
	// 어느 스레드에서 호출해도 안전(DB 스레드, 네트워크 스레드 등)
	void Post(Job job) {
		jobQueue_->Push(std::move(job));

		// 큐를 비우는 일을 워커에 예약. 이미 누가 돌리는 중이면 Execute()가 바로 return한다.
		GlobalWorkerPool().Post([self = this->shared_from_this(), queue = jobQueue_]() {
			queue->Execute();
			});
	}

private:
	JobQueueRef jobQueue_ = std::make_shared<JobQueue>();
};