#pragma once
#include <cstddef>

#include "aquarium/Vec2.h"

namespace aquarium {

// A prop as it intersects one swim plane: a disc in that plane's local 2D coordinates.
// The engine side derives centre and radius from the actual placed actor's bounds; nothing in
// this layer knows what a coral or a rock is, or how many of them there are.
struct Obstacle {
    Vec2 center;
    float radius = 0.f;
};

struct ObstacleParams {
    float lookAhead = 150.f;   // cm of travel ahead of the fish that is considered
    float margin = 20.f;       // cm of clearance added to every radius
};

// Steers `dir` around the nearest threatening obstacle while preserving its magnitude.
//
// This has the same shape as SteerAlongBoundary, and for the same hard-won reason: killing the
// component of the desired direction that points at an obstacle drops the desired speed to zero,
// so the velocity decelerates through zero and re-accelerates the other way. A velocity that
// passes through zero has no stable heading, which flipped the visible facing by 180 degrees in
// one step (M1 review, fixed in M3). Every return path here is either `dir` unchanged or a
// rotated vector of the SAME length, so a fish never stalls in front of a coral.
//
// It is also deliberately not a physics response: a physics response acts AFTER contact, which
// is by definition a bounce. Steering before contact is what makes the fish look like it saw the
// coral coming.
inline Vec2 SteerAroundObstacles(Vec2 pos, Vec2 dir, const Obstacle* obstacles, std::size_t count,
                                 const ObstacleParams& p)
{
    const float len = dir.Length();
    if (len <= 1e-6f || obstacles == nullptr || count == 0) return dir;
    const Vec2 fwd = dir * (1.f / len);
    const Vec2 side{-fwd.y, fwd.x};   // left of the heading

    float bestAhead = p.lookAhead;
    const Obstacle* best = nullptr;
    float bestLateral = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        const Obstacle& o = obstacles[i];
        const float reach = o.radius + p.margin;
        if (reach <= 0.f) continue;
        const Vec2 off = o.center - pos;
        const float ahead = off.x * fwd.x + off.y * fwd.y;
        if (ahead < -reach || ahead > p.lookAhead) continue;   // behind, or too far to matter yet
        const float lateral = off.x * side.x + off.y * side.y;
        const float lateralAbs = lateral < 0.f ? -lateral : lateral;
        if (lateralAbs >= reach) continue;                     // the heading ray misses it
        if (ahead >= bestAhead) continue;                      // a nearer threat already won
        bestAhead = ahead;
        best = &o;
        bestLateral = lateral;
    }
    if (best == nullptr) return dir;

    // Push sideways, away from the obstacle centre, by as much as the overlap demands: nothing at
    // the rim, a full 45 degrees dead-on. A fish that is exactly dead-on (lateral == 0) has no
    // preferred side, so it always takes the left one. An arbitrary but STABLE choice beats a coin
    // flip that could flutter between frames and shake the fish.
    const float reach = best->radius + p.margin;
    const float lateralAbs = bestLateral < 0.f ? -bestLateral : bestLateral;
    const float push = (reach - lateralAbs) / reach;           // 0 at the rim, 1 dead-on
    const float sign = bestLateral > 0.f ? -1.f : 1.f;         // away from the centre
    const Vec2 turned = fwd + side * (sign * push);
    const Vec2 unit = turned.Normalized();
    if (unit.Length() <= 1e-6f) return dir;                    // unreachable (|turned| >= 1), kept
    return unit * len;                                         // magnitude preserved, always
}

} // namespace aquarium
