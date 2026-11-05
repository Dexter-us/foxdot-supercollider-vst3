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
}
