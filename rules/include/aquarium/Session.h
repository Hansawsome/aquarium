#pragma once
#include <string>

#include "aquarium/FishCatalog.h"
#include "aquarium/Nickname.h"

namespace aquarium {

enum class BeginResult { Ok, InvalidNickname, EmptyCatalog, AlreadyActive };

class SessionManager {
public:
    BeginResult Begin(const std::string& rawNickname, size_t catalogSize, const PickFn& pick) {
        if (active_) return BeginResult::AlreadyActive;
        const NicknameResult n = ValidateNickname(rawNickname);
        if (!n.ok) return BeginResult::InvalidNickname;
        const auto idx = PickFishIndex(catalogSize, pick);
        if (!idx) return BeginResult::EmptyCatalog;
        nickname_ = n.value;
        ownedFishIndex_ = *idx;
        active_ = true;
        return BeginResult::Ok;
    }

    void End() {
        active_ = false;
        nickname_.clear();
        ownedFishIndex_ = 0;
    }

    bool HasActiveSession() const { return active_; }
    const std::string& Nickname() const { return nickname_; }
    size_t OwnedFishIndex() const { return ownedFishIndex_; }

private:
    bool active_ = false;
    std::string nickname_;
    size_t ownedFishIndex_ = 0;
};

} // namespace aquarium
