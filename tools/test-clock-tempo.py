#!/usr/bin/env python3
"""Exercise the real MIDI collector and shared audio/MIDI tempo helpers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
source = (plugin / "processor.cpp").read_text()


def extract(marker, semicolon=False):
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + (";" if semicolon else "")


cpp = r"""
#include "clock_tempo_map.h"
#include "clock_swing.h"
#include "player_effect_chain.h"
#include "python_runtime.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
using int32 = std::int32_t;
constexpr std::size_t kPlayerCount = 8;
constexpr std::size_t kMaxPatternEvents = 256;
constexpr std::size_t kMaxMidiEventsPerBlock = 1024;
constexpr std::size_t kMaxPolyphony = 8;
constexpr std::size_t kAudioVoiceCount = kPlayerCount * (kMaxPolyphony * 2 + 1);
constexpr int kSCBlockSize = 64;
constexpr double kAttackSeconds = .01, kReleaseSeconds = .04;
constexpr std::array<int, 58> kSynthIndexToGraphIndex{};
struct PlayerEffectState {};
struct TestProcessor {
"""
cpp += extract("    struct TimeVarControl {", True)
cpp += extract("    struct TimedMidiEvent {", True)
cpp += extract("    struct PatternState {", True)
cpp += extract("    struct ScheduledAudioNote {", True)
cpp += r"""
    double sampleRate_ = 48000;
    std::uint64_t sampleWorldGeneration_ = 0;
    std::array<PatternState, kPlayerCount> activePatterns_{};
    std::array<std::uint64_t, kPlayerCount> patternStartSamples_{};
"""
for marker in (
    "    static bool parseTimeVarControl(",
    "    static float evaluateTimeVar(",
    "    static float evaluateSampleSelection(",
    "    static double tempoAtBeat(",
    "    static double beatAtSeconds(",
    "    double maxTempo(",
    "    static ClockTempoNoteTiming clockNoteTiming(",
    "    static bool usesFoxDotNativeEnvelope(",
    "    static double foxDotEnvelopeDurationSeconds(",
    "    void collectMidiEvents(",
):
    cpp += extract(marker)
audio_start = source.index("    void updateSynthForBlock(")
audio_end = source.index("\n        for (auto& voice : audioVoices_)", audio_start)
cpp += source[audio_start:audio_end].replace(
    "void updateSynthForBlock(", "std::vector<ScheduledAudioNote> scheduleAudioBlock(")
cpp += "\nreturn {desired.begin(), desired.begin() + desiredCount};\n}\n"
cpp += r"""
};

bool near(double a, double b, double tolerance = 1e-8) {
    return std::abs(a - b) < tolerance;
}
ClockTempoMap mapFor(const std::string& token, double bpm) {
    TestProcessor::TimeVarControl control;
    assert(TestProcessor::parseTimeVarControl(
        token, .001, std::numeric_limits<double>::max(), control));
    ClockTempoMap map;
    assert(map.prepare(control, bpm));
    return map;
}
int main() {
    for (double bpm : {.001, 10., 30., 120., 500., 10000.}) {
        auto map = mapFor("-", bpm);
        for (double beat : {0., .25, 1., 16., 1000.}) {
            assert(near(map.secondsAtBeat(beat), beat * 60 / bpm));
            assert(near(map.beatAtSeconds(map.secondsAtBeat(beat)), beat));
        }
    }
    auto step = mapFor("v:60,120:2,2", 60);
    assert(near(step.secondsAtBeat(1), 1));
    assert(near(step.secondsAtBeat(2), 2));
    assert(near(step.secondsAtBeat(3), 2.5));
    assert(near(step.secondsAtBeat(4), 3));
    assert(near(step.secondsAtBeat(8), 6));
    assert(near(step.bpmAtBeat(2), 120));
    assert(near(step.bpmAtBeat(4), 60));
    auto ramp = mapFor("l:60,120:4,4", 60);
    assert(near(ramp.secondsAtBeat(4), 4 * std::log(2.)));
    assert(near(ramp.secondsAtBeat(8), 8 * std::log(2.)));
    assert(near(ramp.bpmAtBeat(2), 90));
    assert(near(ramp.bpmAtBeat(6), 90));
    auto varied = mapFor("l:55,160,85:1.5,3.25,2.75", 55);
    auto fractional = mapFor("v:20,180,90:0.125,0.375,1.5", 20);
    auto equalRamp = mapFor("l:90,90:4,4", 90);
    for (auto map : {step, ramp, varied, fractional, equalRamp}) {
        double previous = -1;
        for (int i = 0; i <= 20000; ++i) {
            const double beat = i * .037;
            const double seconds = map.secondsAtBeat(beat);
            assert(seconds > previous);
            previous = seconds;
            assert(near(map.beatAtSeconds(seconds), beat, 1e-7));
        }
    }
    // FoxDot snapshots delay/sus using the trigger's beat_dur. A held note at
    // beat 1 (60 BPM) still lasts 3 seconds across the change at beat 2.
    auto note = step.noteTiming(1, .5, 3);
    assert(near(note.startSeconds, 1.5));
    assert(near(note.sustainSeconds, 3));
    note = step.noteTiming(2, .5, 3);
    assert(near(note.startSeconds, 2.25));
    assert(near(note.sustainSeconds, 1.5));

    TestProcessor processor;
    for (std::size_t player = 0; player < 2; ++player) {
        auto& pattern = processor.activePatterns_[player];
        pattern.active = true;
        pattern.bpm = 60;
        pattern.lengthBeats = player == 0 ? 2 : 3;
        pattern.eventCount = static_cast<std::size_t>(pattern.lengthBeats);
        pattern.tempoMap = step;
        processor.patternStartSamples_[player] = 100;
        for (std::size_t i = 0; i < pattern.eventCount; ++i) {
            pattern.events[i].beat = static_cast<double>(i);
            pattern.events[i].durationBeats = .25;
            pattern.events[i].midiNote = 60 + static_cast<int>(i);
        }
    }
    std::vector<std::uint64_t> onsets[2];
    std::array<TestProcessor::TimedMidiEvent, kMaxMidiEventsPerBlock> events{};
    // Small blocks test boundary inclusion and duplicate/missed trigger bugs.
    for (std::uint64_t start = 0; start < 6 * 48000; start += 127) {
        std::size_t count = 0;
        processor.collectMidiEvents(start, std::min<std::uint64_t>(
            start + 127, 6 * 48000), events, count);
        for (std::size_t i = 0; i < count; ++i)
            if (events[i].noteOn)
                onsets[events[i].playerIndex].push_back(events[i].sample);
    }
    const std::vector<std::uint64_t> expected{
        100, 48100, 96100, 120100, 144100, 192100, 240100, 264100};
    assert(onsets[0] == expected && onsets[1] == expected);
    // The actual collector must match the timing helper used by the audio
    // scheduler, including delays and gate ends that cross a tempo transition.
    auto& pattern = processor.activePatterns_[0];
    processor.activePatterns_[1].active = false;
    pattern.eventCount = 1;
    pattern.lengthBeats = 8;
    pattern.events[0].beat = 1;
    pattern.events[0].durationBeats = 3;
    pattern.events[0].delayBeats = .5;
    std::size_t count = 0;
    processor.collectMidiEvents(100, 5 * 48000 + 100, events, count);
    assert(count == 2);
    assert(events[0].noteOn && events[0].sample == 72100);
    assert(!events[1].noteOn && events[1].sample == 216100);
    auto audio = processor.scheduleAudioBlock(72100 - 32);
    assert(audio.size() == 1 && audio[0].startSample == 72100);
    assert(audio[0].sustainEndSample == 216100 && audio[0].endSample == 216100);
    assert(near(audio[0].beatDurationSeconds, 1.0));
    // A note triggered before acceleration keeps sounding after the tempo
    // boundary instead of being cut short by a beat-domain lookup window.
    audio = processor.scheduleAudioBlock(150000);
    assert(audio.size() == 1 && audio[0].startSample == 72100);
    pattern.synthIndex = 57;
    auto& sample = pattern.events[0];
    sample.sampleBuffer = 1;
    sample.sampleVariantCount = 1;
    sample.sampleVariantBuffers[0] = 1;
    sample.sampleVariantDurations[0] = 1;
    sample.sampleLoop = sample.sampleGate = true;
    audio = processor.scheduleAudioBlock(150000);
    assert(audio.size() == 1 && audio[0].endSample == 216100);
    sample.sampleLoop = sample.sampleGate = false;
    audio = processor.scheduleAudioBlock(150000);
    assert(audio.empty()); // Natural WAV playback is not stretched by BPM.
    pattern.synthIndex = 0;
    pattern.tempoMap = ramp;
    pattern.bpm = 60;
    const auto audioTiming = TestProcessor::clockNoteTiming(pattern, 1, .5, 3);
    processor.collectMidiEvents(100, 8 * 48000, events, count);
    assert(count >= 2);
    assert(events[0].sample ==
           100 + static_cast<std::uint64_t>(std::llround(audioTiming.startSeconds * 48000)));
    assert(events[1].sample == events[0].sample +
           static_cast<std::uint64_t>(std::llround(audioTiming.sustainSeconds * 48000)));
    audio = processor.scheduleAudioBlock(events[0].sample);
    assert(audio.size() == 1 && audio[0].startSample == events[0].sample &&
           audio[0].sustainEndSample == events[1].sample);

    TestProcessor::TimeVarControl control;
    for (const auto& bad : {"v:0,120:4,4", "l:-1,120:4,4", "v:inf:4",
                            "v:nan:4", "v:120:0", "l:120:4097"}) {
        assert(!TestProcessor::parseTimeVarControl(
            bad, .001, std::numeric_limits<double>::max(), control));
    }
    std::cout << "PASS: tempo maps, 100005 round trips, actual audio/MIDI "
                 "schedulers, sample gates, loop phase and malformed controls\n";
}
"""
# Fail if audio scheduling accidentally drifts back to the old scalar path.
audio = extract("    void updateSynthForBlock(")
assert "const auto timing = clockNoteTiming(" in audio
assert "timing.startSeconds * sampleRate_" in audio
assert "timing.sustainSeconds" in audio
assert "samplesPerBeat" not in audio

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "tempo.cpp", directory / "tempo"
    unit.write_text(cpp)
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-I", str(plugin), str(unit), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True, timeout=45)
