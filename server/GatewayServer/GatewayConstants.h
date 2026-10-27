#pragma once
#include <cstddef>
#include <cstdint> 


namespace GatewayConst
{
inline constexpr std::size_t CLIENT_RECV_BUFFER_SIZE = 65536;   // 클라이언트 세션 수신 버퍼
inline constexpr std::size_t ZONE_RECV_BUFFER_SIZE = 65536 * 2; // 내부 백본용 대용량 버퍼 (128KB)

inline constexpr uint32_t START_ZONE_ID = 1;
inline constexpr float START_X = 0.f;
inline constexpr float START_Y = 0.f;
inline constexpr float START_Z = 0.f;

} // namespace GatewayConst