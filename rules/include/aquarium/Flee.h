#pragma once
#include "aquarium/Vec2.h"

namespace aquarium {

enum class BehaviorState { Normal, Fleeing, Recovering };

struct FleeParams {
    float fleeDuration = 0.8f;
    float recoverDuration = 1.2f;
    // F-10 says the fish rotates AND accelerates. Direction alone reads as a calm course change
    // at a background fish's 40 cm/s, not as being startled. accel is deliberately NOT scaled:
    // raising acceleration too would spike the velocity on the first frame, which is exactly the
    // "sliding" F-13 forbids.
    float fleeSpeedScale = 2.2f;
};

// Last-resort direction when every fallback is degenerate. Any fixed unit vector will do; what
// matters is that this function never returns {0,0}, because the engine reads a zero desired
// direction as "no input" and decelerates -- a fish that stops when startled.
inline Vec2 DefaultFleeDirection() { return {1.f, 0.f}; }

// Direction away from touch; falls back to current heading, then to fallbackDir, then to a fixed
// unit vector. Guaranteed to be a unit vector.
inline Vec2 ComputeFleeDirection(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir) {
    const Vec2 away = (fishPos - touch).Normalized();
    if (away.Length() > 0.f) return away;
    const Vec2 heading = velocity.Normalized();
    if (heading.Length() > 0.f) return heading;
    const Vec2 fb = fallbackDir.Normalized();
    if (fb.Length() > 0.f) return fb;
    return DefaultFleeDirection();
}

class FleeStateMachine {
public:
    void Touch(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir, const FleeParams& p) {
        if (state_ == BehaviorState::Fleeing) return;   // F-11: ignore re-touch mid-flee
        fleeDir_ = ComputeFleeDirection(touch, fishPos, velocity, fallbackDir);
        state_ = BehaviorState::Fleeing;
        timer_ = p.fleeDuration;
        recoverDuration_ = p.recoverDuration;
    }

    void Step(float dt) {
        if (state_ == BehaviorState::Normal || dt <= 0.f) return;
        timer_ -= dt;
        if (timer_ > 0.f) return;
        if (state_ == BehaviorState::Fleeing) {
            state_ = BehaviorState::Recovering;
            timer_ += recoverDuration_;
            if (timer_ <= 0.f) state_ = BehaviorState::Normal;
        } else {
            state_ = BehaviorState::Normal;
        }
    }

    // F-12: flee overrides player input; recovery restores it.
    Vec2 EffectiveInput(Vec2 playerInput) const {
        return state_ == BehaviorState::Fleeing ? fleeDir_ : playerInput;
    }

    // Multiplier applied to the fish's max speed (F-10). Flat through the flee, then linear back
    // to 1 across recovery: dropping to 1 in one step at the end of the flee reads as a brake.
    float SpeedScale(const FleeParams& p) const {
        if (state_ == BehaviorState::Fleeing) return p.fleeSpeedScale;
        if (state_ == BehaviorState::Recovering && recoverDuration_ > 0.f) {
            const float t = timer_ / recoverDuration_;       // 1 at the start, 0 at the end
            const float clamped = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            return 1.f + (p.fleeSpeedScale - 1.f) * clamped;
        }
        return 1.f;
    }

    BehaviorState State() const { return state_; }
    Vec2 FleeDirection() const { return fleeDir_; }

private:
    BehaviorState state_ = BehaviorState::Normal;
    Vec2 fleeDir_;
    float timer_ = 0.f;
    float recoverDuration_ = 0.f;
};

} // namespace aquarium
