#pragma once
#include <algorithm>

#include "aquarium/Vec2.h"

namespace aquarium {

struct Rect {
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
};

// Kills the outward axis component of `dir` when within avoidDistance of an edge.
inline Vec2 AvoidBoundary(Vec2 pos, Vec2 dir, Rect area, float avoidDistance) {
    if (dir.x > 0.f && pos.x > area.maxX - avoidDistance) dir.x = 0.f;
    if (dir.x < 0.f && pos.x < area.minX + avoidDistance) dir.x = 0.f;
    if (dir.y > 0.f && pos.y > area.maxY - avoidDistance) dir.y = 0.f;
    if (dir.y < 0.f && pos.y < area.minY + avoidDistance) dir.y = 0.f;
    return dir;
}

// Steers `dir` away from the walls while preserving its magnitude.
//
// WHY this exists next to AvoidBoundary: zeroing the outward component (AvoidBoundary) drops the
// desired speed to zero as a fish swims straight at a wall, so the velocity decelerates through
// zero and re-accelerates the other way. A velocity that passes through zero has no stable
// heading, which flipped the visible facing by 180 degrees in a single step (see the M1 review).
// Sliding along the wall at the ORIGINAL magnitude keeps the velocity non-zero and the heading
// continuous, so the fish grazes the wall instead of stalling and snapping around.
inline Vec2 SteerAlongBoundary(Vec2 pos, Vec2 dir, Rect area, float avoidDistance) {
    const float len = dir.Length();
    if (len <= 1e-6f) return {0.f, 0.f}; // no desired direction: nothing to steer

    const bool blockedX = (dir.x > 0.f && pos.x > area.maxX - avoidDistance) ||
                          (dir.x < 0.f && pos.x < area.minX + avoidDistance);
    const bool blockedY = (dir.y > 0.f && pos.y > area.maxY - avoidDistance) ||
                          (dir.y < 0.f && pos.y < area.minY + avoidDistance);
    if (!blockedX && !blockedY) return dir;

    const Vec2 centre{0.5f * (area.minX + area.maxX), 0.5f * (area.minY + area.maxY)};
    if (blockedX && blockedY) {
        // Corner: both slide axes are walls, so head for the area centre at the same magnitude.
        const Vec2 inward = (centre - pos).Normalized();
        if (inward.Length() <= 1e-6f) return dir; // exactly at the centre: nothing to turn toward
        return inward * len;
    }

    Vec2 slide = dir;
    if (blockedX) slide.x = 0.f;
    if (blockedY) slide.y = 0.f;
    if (slide.Length() <= 1e-6f) {
        // Straight at the wall: no tangential component survives. Pick the tangent that points
        // back toward the centre rather than returning zero, which would stall the fish again.
        const Vec2 toCentre = centre - pos;
        slide = blockedX ? Vec2{0.f, toCentre.y} : Vec2{toCentre.x, 0.f};
        if (slide.Length() <= 1e-6f) slide = blockedX ? Vec2{0.f, 1.f} : Vec2{1.f, 0.f};
    }
    // The remaining tangential component is shorter than `dir`; restore the original magnitude so
    // the fish keeps its speed while sliding.
    return slide.Normalized() * len;
}

inline Vec2 ClampToArea(Vec2 pos, Rect area) {
    return {std::clamp(pos.x, area.minX, area.maxX),
            std::clamp(pos.y, area.minY, area.maxY)};
}

} // namespace aquarium
