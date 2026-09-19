#include "aquarium/Nickname.h"

namespace aquarium {
namespace {

bool IsAsciiSpace(unsigned char c) { return c == ' '; }
bool IsControl(unsigned char c) { return c < 0x20 || c == 0x7F; }

// Counts UTF-8 codepoints; returns -1 on invalid leading byte pattern.
int CountCodepoints(const std::string& s) {
    int count = 0;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t adv = 0;
        if (c < 0x80) adv = 1;
        else if ((c >> 5) == 0x6) adv = 2;
        else if ((c >> 4) == 0xE) adv = 3;
        else if ((c >> 3) == 0x1E) adv = 4;
        else return -1;
        i += adv;
        if (i > s.size()) return -1;
        ++count;
    }
    return count;
}

} // namespace

NicknameResult ValidateNickname(const std::string& utf8) {
    size_t begin = 0, end = utf8.size();
    while (begin < end && IsAsciiSpace(static_cast<unsigned char>(utf8[begin]))) ++begin;
    while (end > begin && IsAsciiSpace(static_cast<unsigned char>(utf8[end - 1]))) --end;
    const std::string trimmed = utf8.substr(begin, end - begin);

    if (trimmed.empty()) return {false, "", NicknameError::Empty};

    for (unsigned char c : trimmed) {
        if (IsControl(c)) return {false, "", NicknameError::InvalidCharacter};
    }

    const int cp = CountCodepoints(trimmed);
    if (cp < 0) return {false, "", NicknameError::InvalidCharacter};
    if (cp > kMaxNicknameCodepoints) return {false, "", NicknameError::TooLong};

    return {true, trimmed, NicknameError::None};
}

} // namespace aquarium
