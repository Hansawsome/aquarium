#pragma once
#include "aquarium/Vec2.h"

namespace aquarium {

enum class BehaviorState { Normal, Fleeing, Recovering };

struct FleeParams {
    float fleeDuration = 0.8f;
    float recoverDuration = 1.2f;
};

// Direction away from touch; falls back to current heading, then to fallbackDir.
inline Vec2 ComputeFleeDirection(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir) {
    const Vec2 away = (fishPos - touch).Normalized();
    if (away.Length() > 0.f) return away;
    const Vec2 heading = velocity.Normalized();
    if (heading.Length() > 0.f) return heading;
    return fallbackDir.Normalized();
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

    BehaviorState State() const { return state_; }
    Vec2 FleeDirection() const { return fleeDir_; }

private:
    BehaviorState state_ = BehaviorState::Normal;
    Vec2 fleeDir_;
    float timer_ = 0.f;
    float recoverDuration_ = 0.f;
};

} // namespace aquarium
