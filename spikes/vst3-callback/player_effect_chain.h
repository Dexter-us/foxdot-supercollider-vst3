#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

inline constexpr std::size_t kMaxPlayerEffectChain = 4;

inline int playerEffectChainOutputBus(std::size_t player, std::size_t slot,
                                     int outputBase) noexcept {
    return outputBase +
        static_cast<int>((player * kMaxPlayerEffectChain + slot) * 2);
}

inline int playerEffectChainInputBus(std::size_t player, std::size_t slot,
                                    int inputBase, int outputBase) noexcept {
    return slot == 0 ? inputBase + static_cast<int>(player * 2)
                    : playerEffectChainOutputBus(player, slot - 1, outputBase);
}

// A short, allocation-free held-sample transition prevents a topology switch
// from creating a single-sample edge. This does not preserve a replaced delay's
// history; unchanged inserts retain their graph and normal control smoothing.
struct PlayerEffectChainTransition {
    void select(std::uint64_t signature, double sampleRate) noexcept {
        if (signature == signature_)
            return;
        signature_ = signature;
        from_ = last_;
        const double safeRate = std::isfinite(sampleRate)
            ? std::clamp(sampleRate, 1.0, 768000.0) : 48000.0;
        length_ = std::max(1, static_cast<int>(safeRate * 0.01));
        remaining_ = length_;
    }

    std::array<float, 2> process(float left, float right) noexcept {
        std::array<float, 2> result{left, right};
        if (remaining_ > 0) {
            const float gain = 1.0f -
                static_cast<float>(remaining_) / static_cast<float>(length_);
            for (std::size_t channel = 0; channel < result.size(); ++channel)
                result[channel] =
                    from_[channel] * (1.0f - gain) + result[channel] * gain;
            --remaining_;
        }
        last_ = result;
        return result;
    }

private:
    std::uint64_t signature_ = 0;
    std::array<float, 2> last_{};
    std::array<float, 2> from_{};
    int length_ = 1;
    int remaining_ = 0;
};
