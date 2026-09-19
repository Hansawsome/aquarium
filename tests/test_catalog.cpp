#include <catch2/catch_test_macros.hpp>
#include "aquarium/FishCatalog.h"

using namespace aquarium;

TEST_CASE("picks first and last item via injected rng") {
    auto first = PickFishIndex(3, [](size_t) { return size_t{0}; });
    auto last  = PickFishIndex(3, [](size_t b) { return b - 1; });
    REQUIRE(first.has_value());
    REQUIRE(*first == 0);
    REQUIRE(*last == 2);
}

TEST_CASE("empty catalog yields no assignment") {
    bool called = false;
    auto r = PickFishIndex(0, [&](size_t) { called = true; return size_t{0}; });
    REQUIRE_FALSE(r.has_value());
    REQUIRE_FALSE(called);   // rng must not run on empty catalog
}

TEST_CASE("out-of-range rng result is clamped into catalog") {
    auto r = PickFishIndex(3, [](size_t) { return size_t{99}; });
    REQUIRE(r.has_value());
    REQUIRE(*r == 2);
}
