#pragma once
#include <cmath>

// 거리 / 방향 계산 공용 함수
//  - XZ: 높이(Y)를 무시한 수평면 기준
//  - 3D: 높이까지 포함
//  - 범위 비교는 제곱끼리 비교해서 sqrt를 쓰지 않는다
namespace Math
{
inline constexpr float PI = 3.14159265358979f;
inline constexpr float DEG_TO_RAD = PI / 180.f;

// ===== 길이 =====
inline float LengthXZ(float x, float z)
{
    return std::sqrt(x * x + z * z);
}

inline float Length3D(float x, float y, float z)
{
    return std::sqrt(x * x + y * y + z * z);
}

// ===== 두 점 사이 거리의 제곱 =====
inline float DistSqXZ(float ax, float az, float bx, float bz)
{
    const float dx = bx - ax, dz = bz - az;
    return dx * dx + dz * dz;
}

inline float DistSq3D(float ax, float ay, float az, float bx, float by, float bz)
{
    const float dx = bx - ax, dy = by - ay, dz = bz - az;
    return dx * dx + dy * dy + dz * dz;
}

// ===== 사거리 안인지 (range 이하면 true) =====
inline bool IsWithinRangeXZ(float ax, float az, float bx, float bz, float range)
{
    return DistSqXZ(ax, az, bx, bz) <= range * range;
}

inline bool IsWithinRange3D(float ax, float ay, float az, float bx, float by, float bz, float range)
{
    return DistSq3D(ax, ay, az, bx, by, bz) <= range * range;
}

// ===== 값 검증 =====
// 클라이언트가 보낸 좌표/방향에 NaN, 무한대가 섞였는지
inline bool IsFinite3(float x, float y, float z)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

// ===== 방향 =====
// (x, z)를 길이 1로 정규화. 값이 비정상이거나 길이가 거의 0이면 false (방향을 정할 수 없음)
inline bool NormalizeXZ(float& x, float& z)
{
    if (!std::isfinite(x) || !std::isfinite(z))
        return false;

    const float len = LengthXZ(x, z);
    if (len < 0.01f)
        return false;

    x /= len;
    z /= len;
    return true;
}
} // namespace Math
