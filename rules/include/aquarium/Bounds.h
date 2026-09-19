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

inline Vec2 ClampToArea(Vec2 pos, Rect area) {
    return {std::clamp(pos.x, area.minX, area.maxX),
            std::clamp(pos.y, area.minY, area.maxY)};
}

} // namespace aquarium
