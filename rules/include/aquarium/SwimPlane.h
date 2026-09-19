#pragma once
#include <cmath>

#include "aquarium/Vec2.h"

namespace aquarium {

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 Normalized() const {
        const float len = Length();
        if (len <= 1e-6f) return {};
        return {x / len, y / len, z / len};
    }
};

// A 2D swim plane embedded in 3D: origin + right*x + up*y.
struct SwimPlane {
    Vec3 origin;
    Vec3 right{0.f, 1.f, 0.f};
    Vec3 up{0.f, 0.f, 1.f};

    Vec3 ToWorld(Vec2 p) const { return origin + right * p.x + up * p.y; }
    Vec3 Forward(Vec2 velocity) const { return (right * velocity.x + up * velocity.y).Normalized(); }
};

} // namespace aquarium
