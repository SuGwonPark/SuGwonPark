#pragma once
#include "pch.h"
#include "mstcpip.h"
#include "magic_enum/magic_enum.hpp"
#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>


// 연결이 조용할 때 OS가 대신 생존 확인 패킷을 보내도록 설정한다
inline static void EnableKeepAlive(tcp::socket& s) {
	tcp_keepalive ka{};
	ka.onoff = 1;
	ka.keepalivetime = 30'000;      // 마지막 수신 후 30초 조용하면 첫 확인 패킷 (ms)
	ka.keepaliveinterval = 5'000;   // 응답이 없으면 5초 간격으로 재전송 (ms)

	DWORD bytes = 0;
	if (WSAIoctl(s.native_handle(), SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
		nullptr, 0, &bytes, nullptr, nullptr) == SOCKET_ERROR) {
		std::cerr << "[KeepAlive] 설정 실패: " << WSAGetLastError() << std::endl;
	}

}


// Type->string 
template<typename T>
inline std::string_view TypeToString(T value) {
	return magic_enum::enum_name(value);
}

// CSV 문자열 → enum 으로 변환
template<typename T>
inline T ParseCsvEnum(const std::string& text, const char* column) {
	std::optional<T> value = magic_enum::enum_cast<T>(text);
	if (!value || *value == T::None)
		throw std::invalid_argument(std::string("알 수 없는 ") + column + ": '" + text + "'");
	return *value;
}

// None 허용
template<typename T>
inline T ParseCsvEnum2(const std::string& text, const char* column) {
	std::optional<T> value = ParseType<T>(text);
	if (!value)
		throw std::invalid_argument(std::string("알 수 없는 ") + column + ": '" + text + "'");
	return *value;
}

namespace Random {

	// 스레드마다 하나씩 생기는 난수 엔진 (멀티스레드 안전)
	inline std::mt19937& Engine() {
		// 검증용 (항상 같은값이 나온다)
		//thread_local std::mt19937 gen(12345);
		thread_local std::mt19937 gen(std::random_device{}());
		return gen;
	}

	// 정수: min ~ max (둘 다 포함)
	inline int Int(int min, int max) {
		std::uniform_int_distribution<int> dist(min, max);
		return dist(Engine());
	}

	// 실수: min ~ max
	inline float Float(float min, float max) {
		std::uniform_real_distribution<float> dist(min, max);
		return dist(Engine());
	}

	// 확률: p (0.0 ~ 1.0) 확률로 true
	inline bool Chance(double p) {
		std::bernoulli_distribution dist(p);
		return dist(Engine());
	}

	// 만분율 확률: rate / 10000 (기획 데이터에서 많이 씀, 500 = 5%)
	inline bool ChanceRate(int rate, int maxRate = 10000) {
		return Int(1, maxRate) <= rate;
	}

	// 목록에서 하나 고르기 (빈 목록이면 nullptr)
	template<typename T>
	inline const T* Pick(const std::vector<T>& list) {
		if (list.empty()) return nullptr;
		return &list[Int(0, static_cast<int>(list.size()) - 1)];
	}

	// 가중치 뽑기: 인덱스 반환 (가중치 합이 0이면 -1)
	inline int WeightedIndex(const std::vector<int>& weights) {
		int64_t total = 0;
		for (int w : weights) total += (w > 0 ? w : 0);
		if (total <= 0) return -1;

		std::uniform_int_distribution<int64_t> dist(1, total);
		int64_t roll = dist(Engine());
		for (size_t i = 0; i < weights.size(); ++i) {
			if (weights[i] <= 0) continue;
			roll -= weights[i];
			if (roll <= 0) return static_cast<int>(i);
		}
		return -1;
	}

	// 목록 섞기
	template<typename T>
	inline void Shuffle(std::vector<T>& list) {
		std::shuffle(list.begin(), list.end(), Engine());
	}

}