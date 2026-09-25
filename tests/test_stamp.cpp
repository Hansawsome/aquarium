#include <catch2/catch_test_macros.hpp>

#include "aquarium/Stamp.h"

using namespace aquarium;

TEST_CASE("stamping a fish counts it once") {
    StampBook b;
    REQUIRE(b.Count() == 0);
    REQUIRE(b.Stamp(7));
    REQUIRE(b.Count() == 1);
    REQUIRE(b.Has(7));
}

TEST_CASE("stamping the same fish again does not count twice, and is not an error") {
    // 이미 찍은 놈을 또 들이받는 것은 흔한 일이다. 숫자가 두 번 오르면 자랑거리가
    // 거짓이 되고, 거부당하면 벌처럼 읽힌다. 아무 일도 없는 것이 맞다.
    StampBook b;
    REQUIRE(b.Stamp(7));
    REQUIRE_FALSE(b.Stamp(7));
    REQUIRE(b.Count() == 1);
}

TEST_CASE("there is no limit on how many fish can be stamped") {
    StampBook b;
    for (int i = 0; i < 5000; ++i) REQUIRE(b.Stamp(i));
    REQUIRE(b.Count() == 5000);
}

TEST_CASE("the count only ever goes up within a session") {
    StampBook b;
    int previous = 0;
    for (int i = 0; i < 40; ++i) {
        b.Stamp(i % 13);          // 중복이 잔뜩 섞인 현실적인 흐름
        REQUIRE(b.Count() >= previous);
        previous = b.Count();
    }
    REQUIRE(b.Count() == 13);
}

TEST_CASE("leaving resets everything at once") {
    StampBook b;
    b.Stamp(1); b.Stamp(2);
    b.Reset();
    REQUIRE(b.Count() == 0);
    REQUIRE_FALSE(b.Has(1));
}

TEST_CASE("completion is measured against the number of fish that actually exist") {
    // 36을 리터럴로 박으면 배경 물고기 수를 바꾸는 날 완주가 조용히 깨진다(규약 8).
    StampBook b;
    const int total = 4;
    for (int i = 0; i < total; ++i) {
        REQUIRE_FALSE(b.IsComplete(total));
        b.Stamp(i);
    }
    REQUIRE(b.IsComplete(total));
}

TEST_CASE("completion of an empty sea is not claimed") {
    StampBook b;
    REQUIRE_FALSE(b.IsComplete(0));
}

TEST_CASE("completion fires exactly once, so the sea changes once") {
    StampBook b;
    const int total = 2;
    b.Stamp(0);
    REQUIRE_FALSE(b.ConsumeJustCompleted(total));
    b.Stamp(1);
    REQUIRE(b.ConsumeJustCompleted(total));
    REQUIRE_FALSE(b.ConsumeJustCompleted(total));   // 두 번째부터는 조용하다
}

TEST_CASE("the book has no way to remove a single stamp") {
    // 구조로 보장한다. 한 마리를 지우는 함수가 있으면 그것은 실수로 잃는 길이고,
    // 잃는 것은 이 게임에서 금지다. 아래는 그 사실을 사람이 읽도록 적어 둔 단언이다.
    StampBook b;
    b.Stamp(9);
    b.Stamp(9);
    b.Stamp(9);
    REQUIRE(b.Count() == 1);
    REQUIRE(b.Has(9));
}

TEST_CASE("unknown fish are simply not stamped") {
    StampBook b;
    REQUIRE_FALSE(b.Has(123));
}
