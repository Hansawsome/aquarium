#pragma once
#include <cmath>

namespace aquarium {

struct Vec2 {
    float x = 0.f;
    float y = 0.f;

    float Length() const { return std::sqrt(x * x + y * y); }

    Vec2 Normalized() const {
        const float len = Length();
        if (len <= 1e-6f) return {0.f, 0.f};
        return {x / len, y / len};
    }

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

} // namespace aquarium
