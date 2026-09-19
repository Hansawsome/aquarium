#pragma once
#include "aquarium/Vec2.h"

namespace aquarium {

struct KeyState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

inline Vec2 SteeringVector(KeyState k) {
    Vec2 v{
        (k.right ? 1.f : 0.f) - (k.left ? 1.f : 0.f),
        (k.up ? 1.f : 0.f) - (k.down ? 1.f : 0.f),
    };
    return v.Normalized();
}

} // namespace aquarium
