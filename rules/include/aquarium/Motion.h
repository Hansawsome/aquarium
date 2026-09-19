#pragma once
#include <algorithm>

#include "aquarium/Vec2.h"

namespace aquarium {

struct MotionParams {
    float maxSpeed = 100.f;
    float accel = 200.f;
    float decel = 150.f;
    float maxDeltaTime = 0.1f;
};

struct MotionState {
    Vec2 position;
    Vec2 velocity;
    bool paused = false;
};

// Moves current velocity toward target by at most `rate * dt`, then integrates position.
inline void StepMotion(MotionState& s, Vec2 inputDir, const MotionParams& p, float dt) {
    if (s.paused || dt <= 0.f) return;
    dt = std::min(dt, p.maxDeltaTime);

    const bool hasInput = inputDir.Length() > 1e-6f;
    const Vec2 target = inputDir.Normalized() * (hasInput ? p.maxSpeed : 0.f);
    const float rate = (hasInput ? p.accel : p.decel) * dt;

    const Vec2 diff = target - s.velocity;
    const float dist = diff.Length();
    s.velocity = (dist <= rate) ? target : s.velocity + diff.Normalized() * rate;

    if (s.velocity.Length() > p.maxSpeed)
        s.velocity = s.velocity.Normalized() * p.maxSpeed;

    s.position = s.position + s.velocity * dt;
}

} // namespace aquarium
