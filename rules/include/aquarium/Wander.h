#pragma once
#include <cstdint>
#include <random>

#include "aquarium/Bounds.h"
#include "aquarium/Vec2.h"

namespace aquarium {

class WanderBehavior {
public:
    WanderBehavior(uint32_t seed, Rect area, float arriveRadius = 2.f, float targetLifetime = 6.f)
        : rng_(seed), area_(area), arriveRadius_(arriveRadius), targetLifetime_(targetLifetime) {
        PickTarget();
    }

    // Call each frame with the fish position; renews target on arrival or timeout.
    void Update(Vec2 pos, float dt) {
        age_ += dt;
        if ((target_ - pos).Length() <= arriveRadius_ || age_ >= targetLifetime_) PickTarget();
    }

    Vec2 DesiredDirection(Vec2 pos) const { return (target_ - pos).Normalized(); }
    Vec2 Target() const { return target_; }
    void ForceNewTarget() { PickTarget(); }

private:
    void PickTarget() {
        std::uniform_real_distribution<float> dx(area_.minX, area_.maxX);
        std::uniform_real_distribution<float> dy(area_.minY, area_.maxY);
        target_ = {dx(rng_), dy(rng_)};
        age_ = 0.f;
    }

    std::mt19937 rng_;
    Rect area_;
    float arriveRadius_;
    float targetLifetime_;
    float age_ = 0.f;
    Vec2 target_;
};

} // namespace aquarium
