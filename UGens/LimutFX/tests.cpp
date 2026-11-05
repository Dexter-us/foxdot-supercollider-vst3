#include "LimutFXDSP.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using limutfx::DuckState;
using limutfx::PingPongState;

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
}
