#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

struct ClockTempoNoteTiming {
    double startSeconds = 0.0;
    double startBeat = 0.0;
    double sustainSeconds = 0.0;
};

// Prepared off the audio thread: bounded storage, no allocation, and no repeated
// integration of complete tempo cycles while rendering.
struct ClockTempoMap {
    static constexpr std::size_t capacity = 8;
    bool active = false;
    bool linear = false;
    std::size_t count = 0;
    double fallbackBpm = 120.0;
    double cycleBeats = 0.0;
    double cycleSeconds = 0.0;
    std::array<double, capacity> values{};
    std::array<double, capacity> durations{};
    std::array<double, capacity> segmentSeconds{};

    template <typename Control>
    bool prepare(const Control& control, double bpm) {
        *this = {};
        fallbackBpm = bpm;
        if (!std::isfinite(bpm) || bpm < 0.001)
            return false;
        if (!control.active)
            return true;
        if (control.count == 0 || control.count > capacity)
            return false;
        active = true;
        linear = control.linear;
        count = control.count;
        for (std::size_t i = 0; i < count; ++i) {
            values[i] = control.values[i];
            durations[i] = control.durations[i];
            if (!std::isfinite(values[i]) || values[i] < 0.001 ||
                !std::isfinite(durations[i]) || durations[i] <= 0.0 ||
                durations[i] > 4096.0)
                return false;
            cycleBeats += durations[i];
        }
        for (std::size_t i = 0; i < count; ++i) {
            segmentSeconds[i] = secondsInSegment(i, durations[i]);
            if (!std::isfinite(segmentSeconds[i]) || segmentSeconds[i] <= 0.0)
                return false;
            cycleSeconds += segmentSeconds[i];
        }
        return std::isfinite(cycleSeconds) && cycleSeconds > 0.0;
    }

    double bpmAtBeat(double beat) const {
        if (!active)
            return fallbackBpm;
        double position = std::fmod(std::max(0.0, beat), cycleBeats);
        for (std::size_t i = 0; i < count; ++i) {
            if (position < durations[i] || i + 1 == count) {
                if (!linear || count == 1)
                    return values[i];
                const double part = std::clamp(position / durations[i], 0.0, 1.0);
                return values[i] * (1.0 - part) +
                       values[(i + 1) % count] * part;
            }
            position -= durations[i];
        }
        return fallbackBpm;
    }

    double secondsAtBeat(double beat) const {
        beat = std::max(0.0, beat);
        if (!active)
            return beat * 60.0 / fallbackBpm;
        const double cycles = std::floor(beat / cycleBeats);
        double remainder = beat - cycles * cycleBeats;
        double seconds = cycles * cycleSeconds;
        for (std::size_t i = 0; i < count && remainder > 0.0; ++i) {
            if (remainder >= durations[i]) {
                seconds += segmentSeconds[i];
                remainder -= durations[i];
            } else {
                seconds += secondsInSegment(i, remainder);
                break;
            }
        }
        return seconds;
    }

    double beatAtSeconds(double seconds) const {
        seconds = std::max(0.0, seconds);
        if (!active)
            return seconds * fallbackBpm / 60.0;
        const double cycles = std::floor(seconds / cycleSeconds);
        double remainder = seconds - cycles * cycleSeconds;
        double beats = cycles * cycleBeats;
        for (std::size_t i = 0; i < count && remainder > 0.0; ++i) {
            if (remainder >= segmentSeconds[i]) {
                beats += durations[i];
                remainder -= segmentSeconds[i];
            } else {
                beats += beatsInSegment(i, remainder);
                break;
            }
        }
        return beats;
    }

    double maximumBpm() const {
        double maximum = fallbackBpm;
        for (std::size_t i = 0; i < count; ++i)
            maximum = std::max(maximum, values[i]);
        return maximum;
    }

    double minimumBpm() const {
        double minimum = fallbackBpm;
        for (std::size_t i = 0; i < count; ++i)
            minimum = std::min(minimum, values[i]);
        return minimum;
    }

    ClockTempoNoteTiming noteTiming(double beat, double delayBeats,
                                    double sustainBeats,
                                    double nudgeSeconds = 0.0) const {
        // FoxDot converts delay and sus using beat_dur at the trigger. Existing
        // sounds are not time-stretched when the clock changes afterwards.
        const double beatSeconds = 60.0 / bpmAtBeat(beat);
        const double start = std::max(
            0.0, secondsAtBeat(beat) + delayBeats * beatSeconds - nudgeSeconds);
        return {start, beatAtSeconds(start), sustainBeats * beatSeconds};
    }

private:
    bool constantSegment(std::size_t i) const {
        return !linear || count == 1 ||
            std::abs(values[(i + 1) % count] - values[i]) <=
                1e-12 * std::max(values[i], values[(i + 1) % count]);
    }

    double secondsInSegment(std::size_t i, double beats) const {
        if (constantSegment(i))
            return beats * 60.0 / values[i];
        const double difference = values[(i + 1) % count] - values[i];
        const double part = std::clamp(beats / durations[i], 0.0, 1.0);
        const double endBpm = values[i] * (1.0 - part) +
                              values[(i + 1) % count] * part;
        return (60.0 * durations[i]) *
            (std::log(endBpm) - std::log(values[i])) / difference;
    }

    double beatsInSegment(std::size_t i, double seconds) const {
        if (constantSegment(i))
            return std::clamp(seconds * values[i] / 60.0, 0.0, durations[i]);
        const double difference = values[(i + 1) % count] - values[i];
        const double exponent = seconds * difference / (60.0 * durations[i]);
        const double change = std::abs(exponent) < 1.0
            ? values[i] * std::expm1(exponent)
            : std::exp(std::log(values[i]) + exponent) - values[i];
        return std::clamp(durations[i] * (change / difference),
                          0.0, durations[i]);
    }
};
