#pragma once
#include <cmath>

#include "aquarium/Vec2.h"

namespace aquarium {

// Heading of a 2D vector in degrees, atan2(y, x) in (-180, 180]. Zero vector -> 0.
inline float HeadingDeg(Vec2 v) {
    if (v.x == 0.f && v.y == 0.f) return 0.f;
    return std::atan2(v.y, v.x) * (180.f / 3.14159265358979f);
}

// Shortest signed heading change (wrapped into (-180, 180]) divided by dt. dt <= 0 -> 0.
inline float TurnRateDegPerSec(float prevHeadingDeg, float headingDeg, float dt) {
    if (dt <= 0.f) return 0.f;
    float delta = std::fmod(headingDeg - prevHeadingDeg, 360.f);
    if (delta > 180.f) delta -= 360.f;
    if (delta <= -180.f) delta += 360.f;
    return delta / dt;
}

} // namespace aquarium
