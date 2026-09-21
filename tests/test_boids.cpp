#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Boids.h"

using aquarium::BoidNeighbor;
using aquarium::BoidsParams;
using aquarium::BoidsResult;
using aquarium::SchoolingSteer;
using aquarium::BlendSteering;
using aquarium::Vec2;

namespace {
BoidNeighbor Mate(float x, float y, float vx, float vy, int species = 1) {
    BoidNeighbor n;
    n.position = {x, y};
    n.velocity = {vx, vy};
    n.depth = 0.f;
    n.species = species;
    n.avoidOnly = false;
    return n;
}
} // namespace

TEST_CASE("no neighbours produces no steer", "[boids]") {
    const BoidsParams p;
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, nullptr, 0, p);
    CHECK(r.steer.Length() == 0.f);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 0);
}

TEST_CASE("cohesion pulls toward a distant school mate", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 1, p);
    CHECK(r.consideredCount == 1);
    CHECK(r.avoidCount == 0);
    CHECK(r.steer.x > 0.9f);               // toward it
}

TEST_CASE("separation pushes away from a too-close mate", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(10.f, 0.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 1, p);
    CHECK(r.avoidCount == 1);
    CHECK(r.steer.x < 0.f);                // separation outweighs cohesion at 10 cm
}

TEST_CASE("alignment matches the school heading", "[boids]") {
    BoidsParams p;
    p.cohesionWeight = 0.f;                // isolate alignment
    p.separationWeight = 0.f;
    // Two mates at equal and opposite offsets: cohesion cancels, only alignment survives.
    const BoidNeighbor n[] = {Mate(80.f, 0.f, 0.f, 1.f), Mate(-80.f, 0.f, 0.f, 1.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 2, p);
    CHECK(r.consideredCount == 2);
    CHECK(r.steer.y > 0.99f);
}

TEST_CASE("other species never align or cohere", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f, /*species*/ 2)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, /*species*/ 1, n, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.steer.Length() == 0.f);        // 100 cm away, so no separation either
}

TEST_CASE("other species are still separated from", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(20.f, 0.f, 1.f, 0.f, /*species*/ 2)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, /*species*/ 1, n, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 1);
    CHECK(r.steer.x < -0.9f);
}

TEST_CASE("the player fish is avoided but never followed", "[boids]") {
    const BoidsParams p;
    BoidNeighbor player = Mate(90.f, 0.f, 1.f, 0.f, /*species*/ 1); // SAME species on purpose
    player.avoidOnly = true;
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, &player, 1, p);
    CHECK(r.consideredCount == 0);         // never a cohesion/alignment target
    CHECK(r.avoidCount == 1);              // 90 cm is inside avoidOnlyRadius 110
    CHECK(r.steer.x < -0.9f);              // pushed away, so the school opens around the player
}

TEST_CASE("neighbours on a distant plane are ignored", "[boids]") {
    const BoidsParams p;
    BoidNeighbor far_ = Mate(30.f, 0.f, 1.f, 0.f);
    far_.depth = 400.f;                    // 4 m behind, well past depthRadius 120
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, &far_, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 0);
    CHECK(r.steer.Length() == 0.f);
}

TEST_CASE("maxNeighbors caps how many mates contribute", "[boids]") {
    BoidsParams p;
    p.maxNeighbors = 2;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f), Mate(100.f, 10.f, 1.f, 0.f),
                              Mate(100.f, 20.f, 1.f, 0.f), Mate(100.f, 30.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 4, p);
    CHECK(r.consideredCount == 2);
}

TEST_CASE("BlendSteering keeps a unit vector and never returns zero for opposed inputs", "[boids]") {
    const Vec2 own{1.f, 0.f};
    CHECK(BlendSteering(own, {0.f, 0.f}, 0.f).x == 1.f);
    const Vec2 mixed = BlendSteering(own, {0.f, 1.f}, 0.5f);
    CHECK_THAT(mixed.Length(), Catch::Matchers::WithinAbs(1.0, 1e-4));
    CHECK(mixed.x > 0.f);
    CHECK(mixed.y > 0.f);
    // Exactly opposed: the sum is the zero vector, and returning it would let StepMotion
    // decelerate the fish through zero -- the M1/M3 heading-flip bug. Keep the fish's own intent.
    const Vec2 opposed = BlendSteering(own, {-1.f, 0.f}, 0.5f);
    CHECK(opposed.x == 1.f);
}
