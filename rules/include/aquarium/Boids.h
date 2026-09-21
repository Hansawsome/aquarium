#pragma once
#include <cstddef>

#include "aquarium/Vec2.h"

namespace aquarium {

// One other fish as seen by the fish being steered.
//
// `position`/`velocity` are in the SHARED swim frame (x = world Y, y = world Z), not in either
// fish's local plane coordinates. Every swim plane in this game uses right = +Y and up = +Z, so
// adding the plane origin back gives one common 2D frame, and a direction expressed in it is
// usable by any fish without conversion.
//
// `depth` is the world X of the neighbour's plane. Background fish live on 36 parallel planes
// between X = 330 and X = 700, so two fish that overlap on screen can be 3 m apart. Schooling
// them together would look correct head-on and absurd from any other angle.
struct BoidNeighbor {
    Vec2 position;
    Vec2 velocity;
    float depth = 0.f;
    int species = 0;
    // The player's fish. Never followed (no alignment, no cohesion), only avoided, and with a
    // bigger radius, so the school opens up around exactly the fish the child is watching
    // instead of crowding it.
    bool avoidOnly = false;
};

struct BoidsParams {
    float neighborRadius = 140.f;   // cm in the shared swim frame
    float depthRadius = 120.f;      // cm along world X
    float separationRadius = 45.f;
    float avoidOnlyRadius = 110.f;  // separation radius used for avoidOnly neighbours
    float separationWeight = 1.7f;
    float alignmentWeight = 0.6f;
    float cohesionWeight = 0.35f;
    int maxNeighbors = 6;           // hard cap so one dense clump cannot dominate the steer
};

struct BoidsResult {
    Vec2 steer;                  // unit vector, or zero when nothing applied
    int consideredCount = 0;     // school mates that contributed alignment/cohesion
    int avoidCount = 0;          // neighbours of any species that contributed separation
};

inline bool IsSchoolMate(int selfSpecies, const BoidNeighbor& n) {
    return !n.avoidOnly && n.species == selfSpecies;
}

// Separation + alignment + cohesion over `neighbors`, returned as a unit direction.
//
// Cost note: this is deliberately an all-pairs scan with no spatial structure. There are 36
// background fish, so a full frame is 36 x 35 = 1260 iterations of a handful of arithmetic ops.
// A uniform grid would cost more to maintain than it saves at this n. If a measurement ever
// shows this above the ~1.3 fps run-to-run noise floor, THEN add one.
inline BoidsResult SchoolingSteer(Vec2 pos, float depth, int species,
                                  const BoidNeighbor* neighbors, std::size_t count,
                                  const BoidsParams& p)
{
    BoidsResult r;
    if (neighbors == nullptr || count == 0) return r;

    Vec2 sep{0.f, 0.f};
    Vec2 ali{0.f, 0.f};
    Vec2 coh{0.f, 0.f};
    const int cap = p.maxNeighbors > 0 ? p.maxNeighbors : 0;

    for (std::size_t i = 0; i < count; ++i) {
        const BoidNeighbor& n = neighbors[i];
        const Vec2 off = n.position - pos;
        const float dist = off.Length();
        if (dist <= 1e-4f) continue;   // self, or exactly coincident: no direction to use
        const float depthGap = n.depth - depth;
        const float depthAbs = depthGap < 0.f ? -depthGap : depthGap;
        if (depthAbs > p.depthRadius) continue;

        const float sepRadius = n.avoidOnly ? p.avoidOnlyRadius : p.separationRadius;
        if (dist < sepRadius && sepRadius > 0.f) {
            // Inverse falloff: the closer it is, the harder the push away from it.
            sep = sep + off.Normalized() * (-(sepRadius - dist) / sepRadius);
            ++r.avoidCount;
        }
        if (r.consideredCount >= cap) continue;
        if (!IsSchoolMate(species, n)) continue;   // follow your own species only
        if (dist > p.neighborRadius) continue;
        ali = ali + n.velocity.Normalized();
        coh = coh + off;
        ++r.consideredCount;
    }

    if (r.consideredCount > 0) {
        const float inv = 1.f / static_cast<float>(r.consideredCount);
        ali = ali * inv;
        coh = coh * inv;
    }
    const Vec2 sum = sep * p.separationWeight + ali.Normalized() * p.alignmentWeight +
                     coh.Normalized() * p.cohesionWeight;
    r.steer = sum.Normalized();
    return r;
}

// Blends a fish's own desired direction with its schooling steer. weight 0 -> own direction only,
// weight 1 -> school only. The result is always a unit vector, so the caller hands it straight to
// the boundary rule and StepMotion without changing the fish's speed.
inline Vec2 BlendSteering(Vec2 ownDir, Vec2 schoolDir, float weight) {
    if (weight <= 0.f || schoolDir.Length() <= 1e-6f) return ownDir.Normalized();
    if (weight >= 1.f) return schoolDir.Normalized();
    const Vec2 mix = ownDir.Normalized() * (1.f - weight) + schoolDir.Normalized() * weight;
    const Vec2 unit = mix.Normalized();
    // Exactly opposed inputs sum to zero. Returning zero would make StepMotion decelerate the
    // fish through zero speed, and a velocity that passes through zero has no stable heading --
    // the 180-degree facing flip fixed in M1/M3. Keep the fish's own intent instead.
    if (unit.Length() <= 1e-6f) return ownDir.Normalized();
    return unit;
}

} // namespace aquarium
