#pragma once
#include "pch.h"
#include "mstcpip.h"
#include "Common/Protocol/VarEnum.h"
#include "magic_enum/magic_enum.hpp"

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


// string->Type
template<typename T>
inline std::optional<T> ParseType(std::string_view s) {
	return magic_enum::enum_cast<T>(s);
}

// Type->string 
template<typename T>
inline std::string_view TypeToString(T value) {
	return magic_enum::enum_name(value);
}