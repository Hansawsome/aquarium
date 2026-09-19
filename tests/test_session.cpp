#include <catch2/catch_test_macros.hpp>
#include "aquarium/Session.h"

using namespace aquarium;

static size_t PickZero(size_t) { return 0; }

TEST_CASE("valid nickname begins a session owning exactly one fish") {
    SessionManager m;
    auto r = m.Begin("nemo", 3, PickZero);
    REQUIRE(r == BeginResult::Ok);
    REQUIRE(m.HasActiveSession());
    REQUIRE(m.Nickname() == "nemo");
    REQUIRE(m.OwnedFishIndex() == 0);
}

TEST_CASE("double begin is rejected (no duplicate session)") {
    SessionManager m;
    REQUIRE(m.Begin("nemo", 3, PickZero) == BeginResult::Ok);
    REQUIRE(m.Begin("dory", 3, PickZero) == BeginResult::AlreadyActive);
    REQUIRE(m.Nickname() == "nemo");   // first session untouched
}

TEST_CASE("invalid nickname or empty catalog rejects entry") {
    SessionManager m;
    REQUIRE(m.Begin("   ", 3, PickZero) == BeginResult::InvalidNickname);
    REQUIRE(m.Begin("nemo", 0, PickZero) == BeginResult::EmptyCatalog);
    REQUIRE_FALSE(m.HasActiveSession());
}

TEST_CASE("End clears nickname and ownership; re-entry is fresh (F-14)") {
    SessionManager m;
    m.Begin("nemo", 3, PickZero);
    m.End();
    REQUIRE_FALSE(m.HasActiveSession());
    REQUIRE(m.Nickname().empty());
    REQUIRE(m.Begin("dory", 3, [](size_t) { return size_t{2}; }) == BeginResult::Ok);
    REQUIRE(m.Nickname() == "dory");
    REQUIRE(m.OwnedFishIndex() == 2);
}
