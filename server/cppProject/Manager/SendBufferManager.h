#pragma once
#include "Network/SendBuffer.h"


class SendBufferManager {
public:
	static SendBufferManager* GetInstance() {
		static SendBufferManager instance;
		return &instance;
	}

	// TLS 청크 분할 인터페이스
	static SendBufferRef Open(uint32_t size);
	static void Close(uint32_t writeSize);

	// 글로벌 풀 관리
	SendBufferChunkRef Pop();
	void Push(SendBufferChunk* chunk);

private:
	SendBufferManager() = default;
	~SendBufferManager() = default;

	SendBufferChunkRef CreateChunk();

private:
	std::mutex lock_;
	std::vector<SendBufferChunkRef> pool_;
};