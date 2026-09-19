#pragma once
#include <cstddef>
#include <functional>
#include <optional>

namespace aquarium {

using PickFn = std::function<size_t(size_t bound)>; // returns value in [0, bound)

inline std::optional<size_t> PickFishIndex(size_t catalogSize, const PickFn& pick) {
    if (catalogSize == 0) return std::nullopt;
    size_t idx = pick(catalogSize);
    if (idx >= catalogSize) idx = catalogSize - 1;
    return idx;
}

} // namespace aquarium
