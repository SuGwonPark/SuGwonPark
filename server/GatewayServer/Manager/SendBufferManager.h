#pragma once
#include "Network/SendBuffer.h"


class SendBufferManager {
public:
	static SendBufferManager* GetInstance() {
		static SendBufferManager instance;
		return &instance;
	}

	static SendBufferRef Make(const void* data, uint32_t size);

	template<typename THead, typename TItem>
	static SendBufferRef MakeList(THead head, const std::vector<TItem>& items) {
		const size_t total = sizeof(THead) + items.size() * sizeof(TItem);
		// 0xFFFF는 65535로, uint16_t 최대 값
		if (total > 0xFFFF) return nullptr;

		head.header.size = static_cast<uint16_t> (total);

		SendBufferRef buf = Open(static_cast<uint32_t>(total));
		std::memcpy(buf->Buffer(), &head, sizeof(THead));
		if (!items.empty()) {
			std::memcpy(buf->Buffer() + sizeof(THead),
				items.data(), items.size() * sizeof(TItem));
		}
		Close(static_cast<uint32_t>(total));
		return buf;
	}

	// [헤더 + 본문 이어붙이기] head.header.size 는 여기서 채움
	template<typename THead>
	static SendBufferRef MakeWithPayload(THead head, const void* body, uint32_t bodySize) {
		const size_t total = sizeof(THead) + bodySize;
		// 0xFFFF는 65535로, uint16_t 최대 값
		if (total > 0xFFFF) return nullptr;

		head.header.size = static_cast<uint16_t>(total);

		SendBufferRef buf = Open(static_cast<uint32_t>(total));
		std::memcpy(buf->Buffer(), &head, sizeof(THead));
		if (bodySize > 0) {
			std::memcpy(buf->Buffer() + sizeof(THead), body, bodySize);
		}
		Close(static_cast<uint32_t>(total));
		return buf;
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

