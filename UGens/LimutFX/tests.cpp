#include "LimutFXDSP.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using limutfx::DuckState;
using limutfx::PingPongState;
using limutfx::DisperserState;
using limutfx::FuzzState;
using limutfx::GatedVerbState;
using limutfx::ReverbState;
using limutfx::ShifterState;
using limutfx::ShimmerState;
using limutfx::CompressorState;
using limutfx::VocoderState;

static bool near(float left, float right, float epsilon = 1e-5f) {
    return std::fabs(left - right) <= epsilon;
}

int main() {
    {
        constexpr uint32_t frames = 32;
        std::vector<float> left(frames, 0.0f);
        std::vector<float> right(frames, 0.0f);
        PingPongState state{left.data(), right.data(), frames, 0, 10.0f};
        float outLeft = 0.0f;
        float outRight = 0.0f;

        // The dry setting is sample-exact and still advances the ring safely.
        limutfx::pingPongSample(
            state, 0.7f, -0.2f, 0.2f, 0.95f, 0.0f, outLeft, outRight);
        assert(near(outLeft, 0.7f));
        assert(near(outRight, -0.2f));

        state = PingPongState{left.data(), right.data(), frames, 0, 10.0f};
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        for (int frame = 0; frame < 10; ++frame) {
            limutfx::pingPongSample(
                state, frame == 0 ? 1.0f : 0.0f, 0.0f, 0.2f, 0.5f, 1.0f,
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            if (frame < 2)
                assert(near(outLeft, 0.0f) && near(outRight, 0.0f));
            if (frame == 2)
                assert(near(outRight, 1.0f));
            if (frame == 4)
                assert(near(outLeft, 0.5f));
        }

        // NaN controls fall back to bounded defaults rather than contaminating output.
        limutfx::pingPongSample(
            state, std::numeric_limits<float>::quiet_NaN(), 0.0f,
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN(), 1.0f, outLeft, outRight);
        assert(std::isfinite(outLeft) && std::isfinite(outRight));
    }
    {
        assert(near(limutfx::duckGain(0.0f, 0.5f, 0.7f, 0.1f, 0.2f), 0.3f));
        assert(near(limutfx::duckGain(0.1f, 0.5f, 0.7f, 0.1f, 0.2f), 0.3f));
        assert(near(limutfx::duckGain(0.2f, 0.5f, 0.7f, 0.1f, 0.2f), 0.65f));
        assert(near(limutfx::duckGain(0.31f, 0.5f, 0.7f, 0.1f, 0.2f), 1.0f));

        DuckState state{0.0f, 100.0f};
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::duckSample(state, 1.0f, -1.0f, 0.5f, 0.7f, 0.1f, 0.2f,
                            1.0f, outLeft, outRight);
        assert(outLeft > 0.3f && outLeft < 1.0f);
        assert(near(outLeft, -outRight));
        for (int i = 0; i < 1000; ++i) {
            limutfx::duckSample(state, 0.5f, 0.5f, 0.0f, 2.0f, 9.0f, 0.0f,
                                1.0f, outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
        }
        assert(state.phaseSeconds >= 0.0f && state.phaseSeconds < 0.5f);
    }
    {
        DisperserState state;
        state.sampleRate = 48000.0f;
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::disperserSample(
            state, 0.75f, -0.25f, 440.0f, 3.0f, 8.0f, 0.0f,
            outLeft, outRight);
        assert(near(outLeft, 0.75f) && near(outRight, -0.25f));

        state = DisperserState{};
        state.sampleRate = 48000.0f;
        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        bool differsFromDry = false;
        for (int frame = 0; frame < 12000; ++frame) {
            const float input = frame == 0 ? 0.8f : 0.0f;
            limutfx::disperserSample(
                state, input, -input, 730.0f, 2.5f, 12.0f, 1.0f,
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(outLeft) * outLeft;
            differsFromDry |= std::fabs(outLeft - input) > 1e-4f;
            assert(near(outLeft, -outRight, 2e-5f));
        }
        assert(differsFromDry);
        assert(outputEnergy > inputEnergy * 0.95 &&
               outputEnergy < inputEnergy * 1.05);

        for (int frame = 0; frame < 1000; ++frame) {
            limutfx::disperserSample(
                state, 0.2f, 0.1f,
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), 1000.0f,
                std::numeric_limits<float>::quiet_NaN(), outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
        }
        assert(state.currentStages >= 1.0f &&
               state.currentStages <= limutfx::kMaximumDisperserStages);
    }
    {
        FuzzState state;
        state.sampleRate = 48000.0f;
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::fuzzSample(state, 0.7f, -0.3f, 30.0f, 10000.0f,
                            0.0f, outLeft, outRight);
        assert(near(outLeft, 0.7f) && near(outRight, -0.3f));

        state = FuzzState{};
        state.sampleRate = 48000.0f;
        double inputError = 0.0;
        double distortionError = 0.0;
        float peak = 0.0f;
        for (int frame = 0; frame < 4800; ++frame) {
            const float input =
                0.8f * std::sin(6.2831853071795864769f * 220.0f *
                                static_cast<float>(frame) / 48000.0f);
            limutfx::fuzzSample(
                state, input, input, 36.0f, 9000.0f, 1.0f,
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            assert(near(outLeft, outRight, 2e-5f));
            inputError += static_cast<double>(outLeft - input) *
                          (outLeft - input);
            distortionError += static_cast<double>(input) * input;
            peak = std::max(peak, std::fabs(outLeft));
        }
        assert(inputError > distortionError * 0.01);
        assert(peak <= 1.001f);

        for (int frame = 0; frame < 1000; ++frame) {
            limutfx::fuzzSample(
                state, 0.2f, -0.2f,
                std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN(),
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
        }
    }
    {
        constexpr float sampleRate = 48000.0f;
        const auto pitchFrames = static_cast<uint32_t>(
            std::ceil(sampleRate * limutfx::kPitchBufferSeconds)) + 4u;
        std::vector<float> pitchLeft(pitchFrames, 0.0f);
        std::vector<float> pitchRight(pitchFrames, 0.0f);
        ShifterState state;
        state.left = pitchLeft.data();
        state.right = pitchRight.data();
        state.frames = pitchFrames;
        state.sampleRate = sampleRate;
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::shifterSample(state, 0.25f, -0.25f, 0.0f, 2.0f,
                               0.1f, outLeft, outRight);
        assert(near(outLeft, 0.25f) && near(outRight, -0.25f));

        state = ShifterState{};
        state.left = pitchLeft.data();
        state.right = pitchRight.data();
        state.frames = pitchFrames;
        state.sampleRate = sampleRate;
        int positiveCrossings = 0;
        float previous = 0.0f;
        constexpr int firstFrame = 12000;
        constexpr int lastFrame = 24000;
        for (int frame = 0; frame < lastFrame; ++frame) {
            const float input = 0.5f * std::sin(
                6.2831853071795864769f * 220.0f * frame / sampleRate);
            limutfx::shifterSample(state, input, input, 1.0f, 2.0f,
                                   0.1f, outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            assert(near(outLeft, outRight, 2e-5f));
            if (frame >= firstFrame && previous <= 0.0f && outLeft > 0.0f)
                ++positiveCrossings;
            previous = outLeft;
        }
        const float measuredHz = positiveCrossings * sampleRate /
                                 (lastFrame - firstFrame);
        assert(measuredHz > 390.0f && measuredHz < 490.0f);
    }
    {
        constexpr float sampleRate = 48000.0f;
        const auto pitchFrames = static_cast<uint32_t>(
            std::ceil(sampleRate * limutfx::kPitchBufferSeconds)) + 4u;
        std::vector<float> pitchLeft(pitchFrames, 0.0f);
        std::vector<float> pitchRight(pitchFrames, 0.0f);
        ShimmerState shimmer;
        shimmer.sampleRate = sampleRate;
        shimmer.pitch.left = pitchLeft.data();
        shimmer.pitch.right = pitchRight.data();
        shimmer.pitch.frames = pitchFrames;
        shimmer.pitch.sampleRate = sampleRate;
        shimmer.reverb.sampleRate = sampleRate;
        limutfx::prepareReverb(shimmer.reverb);
        std::array<std::vector<float>, 8> reverbLines;
        for (std::size_t channel = 0; channel < 2; ++channel)
            for (std::size_t comb = 0; comb < limutfx::kReverbCombCount; ++comb) {
                const std::size_t index = channel * limutfx::kReverbCombCount +
                                          comb;
                reverbLines[index].assign(
                    shimmer.reverb.lengths[channel][comb], 0.0f);
                shimmer.reverb.buffers[channel][comb] =
                    reverbLines[index].data();
            }
        float outLeft = 0.0f;
        float outRight = 0.0f;
        float peak = 0.0f;
        for (int frame = 0; frame < 48000; ++frame) {
            const float input = frame < 6000
                ? 0.4f * std::sin(
                    6.2831853071795864769f * 220.0f * frame / sampleRate)
                : 0.0f;
            limutfx::shimmerSample(shimmer, input, -input, 1.0f, 2.0f,
                                   0.1f, 0.75f, 0.45f, outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            peak = std::max(peak, std::fabs(outLeft));
        }
        assert(peak > 0.05f);
        assert(std::fabs(outLeft) < 0.02f);
    }
    {
        constexpr float sampleRate = 48000.0f;
        GatedVerbState state;
        state.sampleRate = sampleRate;
        state.reverb.sampleRate = sampleRate;
        limutfx::prepareReverb(state.reverb);
        std::array<std::vector<float>, 8> reverbLines;
        for (std::size_t channel = 0; channel < 2; ++channel)
            for (std::size_t comb = 0; comb < limutfx::kReverbCombCount; ++comb) {
                const std::size_t index = channel * limutfx::kReverbCombCount +
                                          comb;
                reverbLines[index].assign(
                    state.reverb.lengths[channel][comb], 0.0f);
                state.reverb.buffers[channel][comb] =
                    reverbLines[index].data();
            }
        float outLeft = 0.0f;
        float outRight = 0.0f;
        float earlyPeak = 0.0f;
        float latePeak = 0.0f;
        for (int frame = 0; frame < 72000; ++frame) {
            const bool active = frame >= 4800 && frame < 7200;
            const float input = active
                ? 0.5f * std::sin(
                    6.2831853071795864769f * 330.0f * frame / sampleRate)
                : 0.0f;
            limutfx::gatedVerbSample(state, input, input, 1.0f, 0.7f,
                0.5f, 0.015f, 0.12f, 0.01f, 0.12f, outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            if (frame >= 9000 && frame < 24000)
                earlyPeak = std::max(earlyPeak, std::fabs(outLeft));
            if (frame >= 60000)
                latePeak = std::max(latePeak, std::fabs(outLeft));
        }
        assert(earlyPeak > 0.01f);
        assert(latePeak < earlyPeak * 0.2f);
    }
    {
        assert(near(limutfx::compressorReductionDb(
                        -6.0f, -18.0f, 20.0f, 0.0f),
                    10.8f, 1e-3f));
        assert(near(limutfx::compressorReductionDb(
                        -30.0f, -18.0f, 20.0f, 0.0f),
                    0.0f));
        assert(limutfx::compressorReductionDb(
                   -18.0f, -18.0f, 12.0f, 6.0f) > 0.0f);

        constexpr float sampleRate = 48000.0f;
        CompressorState state;
        state.sampleRate = sampleRate;
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::compressorSample(state, 0.7f, -0.2f, 0.0f, -24.0f,
            20.0f, 0.001f, 0.1f, 3.0f, outLeft, outRight);
        assert(near(outLeft, 0.7f) && near(outRight, -0.2f));

        state = CompressorState{};
        state.sampleRate = sampleRate;
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        for (int frame = 0; frame < 24000; ++frame) {
            const float left = std::sin(
                6.2831853071795864769f * 220.0f * frame / sampleRate);
            limutfx::compressorSample(state, left, 0.1f * left, 1.0f,
                -12.0f, 20.0f, 0.001f, 0.1f, 2.0f,
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            if (frame > 12000) {
                peakLeft = std::max(peakLeft, std::fabs(outLeft));
                peakRight = std::max(peakRight, std::fabs(outRight));
            }
        }
        assert(peakLeft < 0.45f && peakLeft > 0.1f);
        assert(peakRight < 0.05f); // linked stereo detector preserves image

        limutfx::compressorSample(state,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN(), 2.0f,
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN(),
            -1.0f, -1.0f, 99.0f, outLeft, outRight);
        assert(std::isfinite(outLeft) && std::isfinite(outRight));
    }
    {
        constexpr float sampleRate = 48000.0f;
        VocoderState state;
        state.sampleRate = sampleRate;
        limutfx::updateVocoderBands(state, 120.0f, 9000.0f, 0.2f);
        float outLeft = 0.0f;
        float outRight = 0.0f;
        limutfx::vocoderSample(state, 0.4f, -0.2f, 0.3f, 0.3f,
            0.0f, 0.01f, 0.1f, 1.5f, outLeft, outRight);
        assert(near(outLeft, 0.4f) && near(outRight, -0.2f));

        state = VocoderState{};
        state.sampleRate = sampleRate;
        limutfx::updateVocoderBands(state, 120.0f, 9000.0f, 0.0f);
        double wetEnergy = 0.0;
        float peak = 0.0f;
        for (int frame = 0; frame < 48000; ++frame) {
            const float time = static_cast<float>(frame) / sampleRate;
            const float carrier = 0.35f * std::sin(
                6.2831853071795864769f * 440.0f * time);
            const float modulator = (std::fmod(time * 8.0f, 1.0f) < 0.45f)
                ? 0.4f : -0.2f;
            limutfx::vocoderSample(state, carrier, carrier,
                modulator, -0.5f * modulator, 1.0f,
                0.005f, 0.08f, 1.5f, outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            if (frame > 12000) {
                wetEnergy += static_cast<double>(outLeft) * outLeft;
                peak = std::max(peak, std::fabs(outLeft));
            }
        }
        assert(wetEnergy > 0.1);
        assert(peak < 1.0f);

        state = VocoderState{};
        state.sampleRate = sampleRate;
        limutfx::updateVocoderBands(state, -500.0f, 50000.0f, 5.0f);
        for (int frame = 0; frame < 1000; ++frame) {
            limutfx::vocoderSample(state, 0.25f, 0.25f,
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 99.0f,
                outLeft, outRight);
            assert(std::isfinite(outLeft) && std::isfinite(outRight));
            assert(state.lowHz >= 40.0f && state.highHz > state.lowHz);
        }

        state = VocoderState{};
        state.sampleRate = 1000.0f;
        limutfx::updateVocoderBands(state, 50000.0f, -1.0f, 0.0f);
        assert(state.lowHz <= 360.0f);
        assert(state.highHz >= state.lowHz * 1.2f);
        assert(state.highHz <= 450.0f);
    }
}
