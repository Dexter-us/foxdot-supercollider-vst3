#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dexter {

// Sardine-style event gate. Bar numbers are counted from the player's pattern
// start in this embedded VST scheduler; `on` masks are 1-based in Python and
// stored as zero-based bits here.
inline bool sardineEventPasses(double probability, std::uint32_t seed,
                               std::size_t playerIndex, std::int64_t cycle,
                               std::size_t eventIndex, double eventBeat,
                               std::uint16_t loaf,
                               std::uint32_t onMask) noexcept {
    if (loaf > 0) {
        const auto bar = static_cast<std::int64_t>(std::floor(eventBeat / 4.0));
        const auto inLoaf = static_cast<unsigned>(bar % loaf);
        if ((onMask & (std::uint32_t{1} << inLoaf)) == 0)
            return false;
    }
    if (probability <= 0.0)
        return false;
    if (probability >= 1.0)
        return true;

    std::uint64_t value = static_cast<std::uint64_t>(seed) +
        0x9e3779b97f4a7c15ULL *
            (static_cast<std::uint64_t>(cycle) + 1ULL) +
        0xd1b54a32d192ed03ULL *
            (static_cast<std::uint64_t>(playerIndex) + 1ULL) +
        0x94d049bb133111ebULL *
            (static_cast<std::uint64_t>(eventIndex) + 1ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value ^= value >> 31;
    const double roll = static_cast<double>(value >> 11) *
                        (1.0 / 9007199254740992.0);
    return roll < probability;
}

} // namespace dexter
