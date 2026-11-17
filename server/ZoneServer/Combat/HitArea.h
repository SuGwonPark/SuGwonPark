#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Util/MathUtil.h"

struct HitArea
{
    enum class Shape : uint8_t
    {
        Circle, // 원
        Cone,   // 부채꼴
        Line    // 직사각형
    };

    Shape shape = Shape::Circle;
    float ox = 0.f, oz = 0.f;     // 기준점 (원: 중심, 부채꼴/직선: 시전자 위치)
    float dirX = 0.f, dirZ = 1.f; // 방향 (정규화된 값)
    float length = 0.f;           // 원/부채꼴: 반지름, 직선: 길이
    float halfWidth = 0.f;        // 직선: 폭의 절반
    float cosHalfAngle = 1.f;     // 부채꼴: 반각의 cos

    static HitArea Circle(float x, float z, float radius)
    {
        HitArea a;
        a.shape = Shape::Circle;
        a.ox = x;
        a.oz = z;
        a.length = radius;
        return a;
    }

    static HitArea Cone(float x, float z, float dirX, float dirZ, float length, float angleDeg)
    {
        HitArea a;
        a.shape = Shape::Cone;
        a.ox = x;
        a.oz = z;
        a.dirX = dirX;
        a.dirZ = dirZ;
        a.length = length;
        a.cosHalfAngle = std::cos(angleDeg * 0.5f * Math::DEG_TO_RAD);
        return a;
    }

    static HitArea Line(float x, float z, float dirX, float dirZ, float length, float width)
    {
        HitArea a;
        a.shape = Shape::Line;
        a.ox = x;
        a.oz = z;
        a.dirX = dirX;
        a.dirZ = dirZ;
        a.length = length;
        a.halfWidth = width * 0.5f;
        return a;
    }

    float BoundRadius() const
    {
        return shape == Shape::Line ? Math::LengthXZ(length, halfWidth) : length;
    }

    bool Contains(float x, float z) const
    {
        const float dx = x - ox, dz = z - oz;
        const float distSq = dx * dx + dz * dz;

        switch (shape)
        {
            case Shape::Circle:
                return distSq <= length * length;

            case Shape::Cone: {
                if (distSq > length * length)
                    return false;
                if (distSq < 1e-6f) // 같은 위치
                    return true;

                // 방향과 이루는 각이 반각 이내인지 체크 dot >= |d| * cos(반각)
                const float dot = dx * dirX + dz * dirZ;
                return dot >= cosHalfAngle * std::sqrt(distSq);
            }

            case Shape::Line: {
                const float forward = dx * dirX + dz * dirZ; // 진행 방향 거리
                const float side = dx * dirZ - dz * dirX;    // 옆 방향 거리
                return forward >= 0.f && forward <= length && std::abs(side) <= halfWidth;
            }
        }
        return false;
    }
};
