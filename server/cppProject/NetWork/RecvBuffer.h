#pragma once
#include "pch.h"

class RecvBuffer {
public:
	RecvBuffer(int32_t bufferSize = 4096, int32_t bufferCount = 10)
		: capacity_(bufferSize* bufferCount), buffer_(bufferSize* bufferCount) {
	}

	// 처리 완료 후 남은 데이터를 버퍼 맨 앞으로 이동 (슬라이딩)
	void Clean() {
		int32_t dataSize = DataSize();
		if (dataSize == 0) {
			readPos_ = 0;
			writePos_ = 0;
		}
		else {
			std::memmove(&buffer_[0], &buffer_[readPos_], dataSize);
			readPos_ = 0;
			writePos_ = dataSize;
		}
	}

	bool OnRead(int32_t numOfBytes) {
		if (numOfBytes > DataSize()) return false;
		readPos_ += numOfBytes;
		return true;
	}

	bool OnWrite(int32_t numOfBytes) {
		if (numOfBytes > FreeSize()) return false;
		writePos_ += numOfBytes;
		return true;
	}

	uint8_t* WritePos() { return &buffer_[writePos_]; }
	uint8_t* ReadPos() { return &buffer_[readPos_]; }

	int32_t DataSize() const { return writePos_ - readPos_; }
	int32_t FreeSize() const { return capacity_ - writePos_; }

private:
	int32_t capacity_;
	int32_t readPos_ = 0;
	int32_t writePos_ = 0;
	std::vector<uint8_t> buffer_;
};