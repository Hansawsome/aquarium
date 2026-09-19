#include <catch2/catch_test_macros.hpp>
#include "aquarium/Nickname.h"

using namespace aquarium;

TEST_CASE("trims surrounding spaces") {
    auto r = ValidateNickname("  nemo  ");
    REQUIRE(r.ok);
    REQUIRE(r.value == "nemo");
}

TEST_CASE("rejects empty and whitespace-only") {
    REQUIRE_FALSE(ValidateNickname("").ok);
    REQUIRE_FALSE(ValidateNickname("   ").ok);
    REQUIRE(ValidateNickname("   ").error == NicknameError::Empty);
}

TEST_CASE("rejects control chars and newlines") {
    REQUIRE_FALSE(ValidateNickname("ne\nmo").ok);
    REQUIRE_FALSE(ValidateNickname("ne\tmo").ok);   // interior tab
    REQUIRE_FALSE(ValidateNickname(std::string("a\x01b", 3)).ok);
    REQUIRE(ValidateNickname("ne\nmo").error == NicknameError::InvalidCharacter);
}

TEST_CASE("hangul counts by codepoint") {
    // 12 hangul syllables = 12 codepoints -> ok
    REQUIRE(ValidateNickname(u8"가나다라마바사아자차카타").ok);
    // 13 -> too long
    auto r = ValidateNickname(u8"가나다라마바사아자차카타파");
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.error == NicknameError::TooLong);
}

TEST_CASE("13 ascii chars rejected, 12 accepted") {
    REQUIRE(ValidateNickname("abcdefghijkl").ok);
    REQUIRE_FALSE(ValidateNickname("abcdefghijklm").ok);
}
