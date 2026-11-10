#pragma once

namespace dexter::sclang {

// Keep the host-driven SC World and the language tab's private ServerOptions
// on the same audio-bus layout.
inline constexpr int kAudioBusChannelCount = 2048;

inline constexpr bool validFirstUserAudioBus(int firstUserAudioBus) noexcept {
    return firstUserAudioBus >= 2 &&
           firstUserAudioBus <= kAudioBusChannelCount - 2;
}

} // namespace dexter::sclang
