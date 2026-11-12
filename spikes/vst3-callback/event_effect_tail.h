#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dexter {

inline constexpr std::size_t kMaxEventEffectTailVoicesPerPlayer = 4;
inline constexpr double kMaxEventEffectTailSeconds = 12.0;
inline constexpr float kEventEffectTailSilenceRms = 1.0e-5f;

struct EventEffectTailState {
    bool active = false;
    bool finished = false;
    std::uint64_t startSample = 0;
    std::uint64_t hardStopSample = 0;
    std::uint64_t quietStartSample = 0;
    std::uint64_t quietGraceSamples = 0;
};

inline double eventEffectTailQuietGraceSeconds(std::string_view name) {
    if (name == "JPverb" || name == "LimutShimmer" ||
        name == "LimutGatedVerb")
        return 0.75;
    if (name == "Greyhole")
        return 1.5;
    if (name == "AirwindowsTapeDelay2" || name == "AirwindowsEchojam" ||
        name == "LimutPingPong")
        return 2.1;
    return 0.0;
}

inline EventEffectTailState beginEventEffectTail(
    std::string_view name, std::uint64_t startSample, double sampleRate) {
    EventEffectTailState state;
    const double quietSeconds = eventEffectTailQuietGraceSeconds(name);
    if (!(sampleRate > 0.0) || !std::isfinite(sampleRate) ||
        quietSeconds <= 0.0)
        return state;
    const auto hardLimit = static_cast<std::uint64_t>(
        std::ceil(sampleRate * kMaxEventEffectTailSeconds));
    const auto quietLimit = static_cast<std::uint64_t>(
        std::ceil(sampleRate * quietSeconds));
    state.active = true;
    state.startSample = startSample;
    state.hardStopSample = startSample + hardLimit;
    state.quietGraceSamples = quietLimit;
    return state;
}

inline bool updateEventEffectTail(EventEffectTailState& state, float rms,
                                  std::uint64_t blockStart,
                                  std::uint64_t blockSamples) {
    if (!state.active || state.finished)
        return state.finished;
    if (blockStart >= state.hardStopSample) {
        state.finished = true;
        return true;
    }
    if (!std::isfinite(rms) || rms > kEventEffectTailSilenceRms) {
        state.quietStartSample = 0;
        return false;
    }
    if (state.quietStartSample == 0)
        state.quietStartSample = blockStart == 0 ? 1 : blockStart;
    const auto quietEnd = blockStart + blockSamples;
    if (quietEnd >= state.quietStartSample &&
        quietEnd - state.quietStartSample >= state.quietGraceSamples)
        state.finished = true;
    return state.finished;
}

} // namespace dexter
