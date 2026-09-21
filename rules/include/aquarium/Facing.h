#pragma once
#include <algorithm>
#include <cmath>

namespace aquarium {

// Separate limits for the two independent parts of a facing change.
//
// "Swing" turns the nose toward the new heading. "Twist" rolls the body about the nose-tail axis.
// They need different limits because of a fact about this game's geometry, diagnosed in the M4c
// design document:
//
//   Every fish swims on a vertical plane, and its facing frame is built from the heading plus
//   world up. Two headings that straddle the vertical -- say a hair short of straight up on one
//   side and a hair past it on the other -- produce frames that differ by a 180 degree TWIST
//   about the (almost vertical) forward axis. Slewing the whole quaternion at a single rate
//   spends that 180 degrees at the turn rate: 180 / 540 = 0.33 s. That is exactly the ~0.3 s
//   pirouette recorded in the M2 review and still present through M4b.
//
// The twist cannot be removed. A plane-bound fish that keeps its dorsal fin up has a frame that
// is necessarily discontinuous at the two vertical headings; that is topology, not a bug. So the
// goal is not to remove it but to SPEND IT WHERE IT CANNOT BE SEEN: while the heading is steep
// the fish is nearly end-on to this game's fixed horizontal camera, and a fast twist of a
// bilaterally symmetric body reads as a flicker rather than a roll.
struct FacingParams {
    float maxTurnRateDegPerSec = 540.f;      // swing: how fast the nose may sweep
    float uprightRollRateDegPerSec = 540.f;  // twist while the heading is shallow (M3 behaviour)
    float steepRollRateDegPerSec = 2880.f;   // twist while steep: 180 deg in 62 ms
    float steepBeginSin = 0.70f;             // |sin(pitch)| where the fast rate starts blending in
};

// 0 while the heading is shallower than steepBeginSin, 1 at exactly vertical, smooth between.
// `verticalSin` is the vertical component of the unit forward vector. Its sign is ignored: a dive
// and a climb are equally end-on to the camera.
inline float Steepness(float verticalSin, const FacingParams& p) {
    const float s = std::min(std::fabs(verticalSin), 1.f);
    const float begin = std::min(std::max(p.steepBeginSin, 0.f), 0.999f);
    if (s <= begin) return 0.f;
    const float t = std::min((s - begin) / (1.f - begin), 1.f);
    return t * t * (3.f - 2.f * t);   // smoothstep, so there is no rate step at the band edge
}

// Degrees the nose may swing this step.
inline float MaxSwingStepDeg(const FacingParams& p, float dt) {
    if (dt <= 0.f) return 0.f;
    return std::max(p.maxTurnRateDegPerSec, 0.f) * dt;
}

// Degrees the body may twist about the forward axis this step.
inline float MaxTwistStepDeg(float verticalSin, const FacingParams& p, float dt) {
    if (dt <= 0.f) return 0.f;
    const float slow = std::max(p.uprightRollRateDegPerSec, 0.f);
    // Clamped up, never down: a params struct that asks for a slower steep rate than the upright
    // rate would reintroduce the very defect this exists to remove.
    const float fast = std::max(p.steepRollRateDegPerSec, slow);
    return (slow + (fast - slow) * Steepness(verticalSin, p)) * dt;
}

} // namespace aquarium
