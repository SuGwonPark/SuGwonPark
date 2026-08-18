// SendBuffer.h
#pragma once
#include "pch.h"

class SendBufferChunk;

// 개별 패킷이 들고 다니는 슬라이스 버퍼
class SendBuffer : public std::enable_shared_from_this<SendBuffer> {
public:
	SendBuffer(std::shared_ptr<SendBufferChunk> owner, uint8_t* buffer, uint32_t allocSize);
	~SendBuffer();

	uint8_t* Buffer() { return buffer_; }
	uint32_t AllocSize() const { return allocSize_; }
	void Write(const void* data, uint32_t size) {
		std::memcpy(buffer_, data, size);
	}

private:
	uint8_t* buffer_;
	uint32_t allocSize_;
	// 청크가 메모리에서 해제되지 않도록 참조 유지
	std::shared_ptr<SendBufferChunk> owner_;
};

using SendBufferRef = std::shared_ptr<SendBuffer>;

// 64KB 크기의 대형 메모리 블록
class SendBufferChunk : public std::enable_shared_from_this<SendBufferChunk> {
	enum { CHUNK_SIZE = 0x10000 }; // 64KB

public:
	SendBufferChunk();
	~SendBufferChunk() = default;

	void Reset();
	SendBufferRef Open(uint32_t allocSize);
	void Close(uint32_t writeSize);

	bool IsOpen() const { return isOpen_; }
	uint8_t* Buffer() { return &buffer_[usedSize_]; }
	uint32_t FreeSize() const { return static_cast<uint32_t>(CHUNK_SIZE - usedSize_); }

private:
	std::array<uint8_t, CHUNK_SIZE> buffer_;
	bool isOpen_ = false;
	uint32_t usedSize_ = 0;
};

using SendBufferChunkRef = std::shared_ptr<SendBufferChunk>;