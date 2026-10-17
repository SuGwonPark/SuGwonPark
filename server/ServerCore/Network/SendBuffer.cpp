#include "pch.h"
#include "SendBuffer.h"

SendBuffer::SendBuffer(std::shared_ptr<SendBufferChunk> owner, uint8_t* buffer, uint32_t allocSize)
	: owner_(owner), buffer_(buffer), allocSize_(allocSize) {
}

SendBuffer::~SendBuffer() {
}

SendBufferChunk::SendBufferChunk() {
	Reset();
}

void SendBufferChunk::Reset() {
	isOpen_ = false;
	usedSize_ = 0;
}

SendBufferRef SendBufferChunk::Open(uint32_t allocSize) {
	if (allocSize > FreeSize()) return nullptr;
	isOpen_ = true;
	return std::make_shared<SendBuffer>(shared_from_this(), Buffer(), allocSize);
}

void SendBufferChunk::Close(uint32_t writeSize) {
	isOpen_ = false;
	usedSize_ += writeSize;
}