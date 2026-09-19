#pragma once
#include <string>

namespace aquarium {

enum class NicknameError { None, Empty, TooLong, InvalidCharacter };

struct NicknameResult {
    bool ok = false;
    std::string value;                     // trimmed nickname when ok
    NicknameError error = NicknameError::None;
};

inline constexpr int kMaxNicknameCodepoints = 12;

NicknameResult ValidateNickname(const std::string& utf8);

} // namespace aquarium
