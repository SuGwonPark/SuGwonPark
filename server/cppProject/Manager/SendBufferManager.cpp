#include "pch.h"
#include "SendBufferManager.h"

// 각 워커 스레드마다 독립적으로 할당/유지되는 로컬 청크 포인터
thread_local SendBufferChunkRef LSendBufferChunk = nullptr;

SendBufferRef SendBufferManager::Open(uint32_t size) {
	if (LSendBufferChunk == nullptr) {
		LSendBufferChunk = SendBufferManager::GetInstance()->Pop();
	}

	// 현재 들고 있는 TLS 청크의 잔여 공간이 부족하면 새 청크로 교체
	if (LSendBufferChunk->FreeSize() < size) {
		LSendBufferChunk = SendBufferManager::GetInstance()->Pop();
	}

	return LSendBufferChunk->Open(size);
}

void SendBufferManager::Close(uint32_t writeSize) {
	if (LSendBufferChunk) {
		LSendBufferChunk->Close(writeSize);
	}
}

SendBufferChunkRef SendBufferManager::Pop() {
	std::lock_guard<std::mutex> lock(lock_);
	if (pool_.empty()) {
		return CreateChunk();
	}

	SendBufferChunkRef chunk = pool_.back();
	pool_.pop_back();
	return chunk;
}

void SendBufferManager::Push(SendBufferChunk* chunk) {
	std::lock_guard<std::mutex> lock(lock_);
	chunk->Reset();

	// 스마트 포인터 커스텀 Deleter를 붙여 다시 풀에 적재
	pool_.push_back(SendBufferChunkRef(chunk, [this](SendBufferChunk* ptr) {
		Push(ptr);
		}));
}

SendBufferChunkRef SendBufferManager::CreateChunk() {
	// 풀에 반납될 때 Push()가 자동 호출되도록 람다 Deleter 연결
	return SendBufferChunkRef(new SendBufferChunk(), [this](SendBufferChunk* ptr) {
		Push(ptr);
		});
}