#!/usr/bin/env python3
"""Round-trip all Portable examples through the actual C++ result parser.

Developer regression test requiring g++; Windows CI also compiles this parser
as part of the plugin. No VST host or external SC server is used here.
"""
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
source_path = plugin / "python_runtime.cpp"
source = source_path.read_text(encoding="utf-8", errors="replace")
processor_source = (plugin / "processor.cpp").read_text(
    encoding="utf-8", errors="replace")
assert '#include "user_synth_control_state.h"' in processor_source
assert "dexter::preserveUserSynthControlOverrides(" in processor_source
sys.argv = ["test-pattern-api.py", str(source_path), str(plugin / "Portable")]
api_tests = runpy.run_path(str(root / "tools/test-pattern-api.py"))
parse_start = source.index("bool parseResult(")
parse_end = source.index("} // namespace", parse_start)
cpp = r"""
#include "python_runtime.h"
#include "clock_swing.h"
#include "airwindows_control_smoothing.h"
#include "foxdot_note_effects.h"
#include "foxdot_bitcrusher.h"
#include "player_effect_chain.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
"""
cpp += "constexpr std::size_t kEmbeddedSynthCount = 58;\n"
cpp += source[parse_start:parse_end]
cpp += r"""
int main(int argc, char** argv) {
    if (argc != 3) return 3;
    std::array<bool, 1024> usedBuses{};
    for (std::size_t player = 0; player < 8; ++player) {
        for (std::size_t slot = 0; slot < kMaxPlayerEffectChain; ++slot) {
            const auto output = playerEffectChainOutputBus(player, slot, 458);
            const auto input = playerEffectChainInputBus(player, slot, 442, 458);
            if (slot == 0 && input != 442 + static_cast<int>(player * 2))
                return 10;
            if (slot > 0 &&
                input != playerEffectChainOutputBus(player, slot - 1, 458))
                return 11;
            if (output < 458 || output + 1 >= 1024 ||
                usedBuses[output] || usedBuses[output + 1])
                return 12;
            usedBuses[output] = usedBuses[output + 1] = true;
        }
    }
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        PlayerEffectChainTransition transition;
        transition.process(0.75f, -0.5f);
        transition.select(1, rate);
        auto previous = transition.process(-0.25f, 0.25f);
        if (previous[0] != 0.75f || previous[1] != -0.5f)
            return 13;
        for (int sample = 0; sample < static_cast<int>(rate * 0.01); ++sample) {
            const auto value = transition.process(-0.25f, 0.25f);
            if (!std::isfinite(value[0]) ||
                std::abs(value[0] - previous[0]) > 0.003f ||
                value[0] < -0.25f || value[0] > previous[0])
                return 14;
            previous = value;
        }
        if (previous[0] != -0.25f || previous[1] != 0.25f)
            return 15;
        transition.select(1, rate); // unchanged routing must not restart a ramp
        if (transition.process(0.1f, 0.2f)[0] != 0.1f)
            return 16;
        transition.select(2, rate);
        transition.process(-0.5f, 0.5f);
        transition.select(3, rate); // rapidly changed route remains continuous
        if (std::abs(transition.process(1.0f, -1.0f)[0] - 0.1f) > 1e-6f)
            return 17;
    }
    const double fxBlockSeconds = 64.0 / 48000.0;
    float smoothedFx = 0.0f;
    const float firstFxStep = smoothAirwindowsFxControl(smoothedFx, 1.0f,
                                                        fxBlockSeconds);
    if (!(firstFxStep > 0.0f && firstFxStep < 0.1f))
        return 5;
    for (int block = 0; block < 12; ++block) {
        const float next = smoothAirwindowsFxControl(smoothedFx, 1.0f,
                                                     fxBlockSeconds);
        if (next < smoothedFx || next > 1.0f)
            return 6;
        smoothedFx = next;
    }
    if (smoothedFx < 0.60f || smoothedFx > 0.72f)
        return 7;
    if (smoothAirwindowsFxControl(0.5f, 2.0f, fxBlockSeconds) > 1.0f)
        return 8;
    if (smoothAirwindowsFxControl(0.5f, 0.2f, 0.0) != 0.2f)
        return 9;
    if (dexter::foxDotCoarseGain(0.1, 2.0f) != 1.0f ||
        dexter::foxDotCoarseGain(0.3, 2.0f) != 0.0f ||
        dexter::foxDotCoarseGain(0.1, 0.0f) != 1.0f)
        return 19;
    for (float formant : {0.0f, 3.5f, 7.0f}) {
        dexter::FoxDotFormantState state;
        double energy = 0.0;
        for (int sample = 0; sample < 4096; ++sample) {
            const float input = 0.25f *
                static_cast<float>(std::sin(sample * 0.13));
            const float output = dexter::foxDotFormantSample(
                state, input, formant, 48000.0);
            if (!std::isfinite(output) || std::abs(output) > 1.0f)
                return 20;
            energy += std::abs(output);
        }
        if (energy < 1.0)
            return 21;
    }
    dexter::FoxDotFormantState bypassState;
    if (dexter::foxDotFormantSample(bypassState, 0.25f, -1.0f, 48000.0) !=
            0.25f ||
        dexter::foxDotFormantSample(bypassState, 0.25f, 0.0f, 4000.0) !=
            0.25f)
        return 22;
    if (dexter::quantizeFoxDotBits(-1.0f, 1.0f) != -1.0f ||
        dexter::quantizeFoxDotBits(1.0f, 24.0f) != 1.0f ||
        std::abs(dexter::quantizeFoxDotBits(0.251f, 8.0f) - 0.251f) >
            1.0f / 255.0f)
        return 23;
    dexter::FoxDotBitcrusherState crushState;
    const auto crushedFirst = dexter::processFoxDotBitcrusher(
        crushState, 0.25f, -0.25f, 8.0f, 128.0f, 48000.0, 1.0f);
    const auto crushedHeld = dexter::processFoxDotBitcrusher(
        crushState, -0.5f, 0.5f, 8.0f, 128.0f, 48000.0, 1.0f);
    if (crushedFirst[0] != crushedHeld[0] ||
        crushedFirst[1] != crushedHeld[1] ||
        crushedFirst[0] == 0.25f)
        return 24;
    dexter::FoxDotBitcrusherState fullRateState;
    const auto fullRateFirst = dexter::processFoxDotBitcrusher(
        fullRateState, 0.25f, -0.25f, 8.0f, 1.0f, 96000.0, 1.0f);
    const auto fullRateNext = dexter::processFoxDotBitcrusher(
        fullRateState, -0.5f, 0.5f, 8.0f, 1.0f, 96000.0, 1.0f);
    if (fullRateFirst[0] == fullRateNext[0] ||
        fullRateFirst[1] == fullRateNext[1])
        return 27;
    const auto bypassed = dexter::processFoxDotBitcrusher(
        crushState, 0.125f, -0.375f, 0.0f, 12.0f, 48000.0, 1.0f);
    if (bypassed[0] != 0.125f || bypassed[1] != -0.375f)
        return 25;
    const auto nonfinite = dexter::processFoxDotBitcrusher(
        crushState, std::numeric_limits<float>::quiet_NaN(), 0.5f,
        8.0f, 4.0f, 48000.0, 1.0f);
    if (!std::isfinite(nonfinite[0]) || !std::isfinite(nonfinite[1]))
        return 26;
    if (std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.0)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.25)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.5) - 0.2) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.75) - 0.2) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 1.0)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(-0.1, 120.0, 0.5) + 0.2) > 1e-9 ||
        foxDotClockSwingOffsetBeats(0.0, 120.0, 0.5) != 0.0)
        return 4;
    if (std::abs(foxDotCutSustainSeconds(2.0, 0.25) - 0.5) > 1e-12 ||
        std::abs(foxDotCutSustainSeconds(2.0, 2.0) - 2.0) > 1e-12 ||
        std::abs(foxDotCutEnvelopeSeconds(4.0, 2.0, 0.25) - 0.51) > 1e-12 ||
        foxDotCutEnvelopeSeconds(4.0, 2.0, 1.0) != 4.0 ||
        std::abs(foxDotCutEnvelopeSeconds(4.0, 2.0, 0.0) - 0.01) > 1e-12)
        return 18;
    PythonPattern pattern;
    std::string error;
    if (!parseResult(argv[1], pattern,
                     static_cast<std::size_t>(std::stoul(argv[2])), error)) {
        std::cerr << error << '\n';
        return 1;
    }
    for (const auto& disk : pattern.diskCommands)
        std::cout << "D:" << disk.mode << ':' << disk.slot << ':' << disk.path
                  << ':' << disk.rate << ':' << disk.seconds << ' ';
    for (const auto& player : pattern.players) {
        std::cout << player.synthIndex << ' ';
        std::cout << "A:" << player.playerEffectUserIndex << ' ';
        std::cout << "J:" << player.playerEffects.size() << ':';
        for (const auto& effect : player.playerEffects) {
            std::cout << effect.userSynthIndex << '[';
            for (const auto& token : effect.controlTokens)
                std::cout << token << ',';
            std::cout << ']';
        }
        std::cout << ' ';
        std::cout << "B:";
        for (const auto value : player.playerEffectControlValues)
            std::cout << value << ',';
        std::cout << ' ';
        std::cout << "W:" << player.swingSeconds << ' ';
        std::cout << "H:" << player.shape << ':' << player.drive << ':'
                  << player.shapeControlToken << ':'
                  << player.driveControlToken << ' ';
        std::cout << "V:" << player.bits << ':' << player.crush << ':'
                  << player.bitsControlToken << ':'
                  << player.crushControlToken << ' ';
        std::cout << "T:" << player.bpmControlToken << ' ';
        if (player.playerEffectUserIndex >= 0) {
            std::cout << "X:";
            for (const auto& token : player.effectControlTokens)
                std::cout << token << ',';
            std::cout << ' ';
        }
        if (player.synthIndex == 57)
            std::cout << "Q:" << player.sampleControlToken << ':'
                      << player.sampleRateControlToken << ' ';
        for (const auto& event : player.events)
            std::cout << "N:" << event.vibDepth << ':' << event.slide << ':'
                      << event.slideFrom << ':' << event.slideDelay << ':'
                      << event.bend << ':' << event.bendDelay << ':'
                      << event.chop << ':' << event.spin << ':'
                      << event.cut << ':' << event.coarse << ':'
                      << event.formant << ':' << event.pshift << ' ';
        std::cout << "Y:";
        for (const auto& token : player.noteEffectControlTokens)
            std::cout << token << ',';
    }
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    line_edit_binary = directory / "test-player-line-edit"
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        str(plugin / "tools/test-player-line-edit.cpp"),
        "-o", str(line_edit_binary)
    ], check=True)
    subprocess.run([str(line_edit_binary)], check=True)
    unit, binary = directory / "parse.cpp", directory / "parse"
    unit.write_text(cpp)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(plugin), str(unit), "-o", str(binary)], check=True)
    examples = sorted((plugin / "Portable").glob("*.py"))
    space_portable = root / "UGens/SpaceReverbs/Portable"
    examples += sorted(space_portable.glob("*.py"))
    for example in examples:
        result = directory / "result.out"
        user_synth_names = (
            ("Chorus",) if example.name == "168-chorus-stereo-width.py" else
            ("Flanger",) if example.name == "169-flanger-sweep.py" else
            ("Chorus", "Flanger")
            if example.name == "170-chorus-flanger-together.py" else
            ("Phaser",) if example.name == "171-phaser-sweep.py" else
            ("Tremolo",) if example.name == "172-tremolo-motion.py" else
            ("Phaser", "Tremolo") if example.name == "173-ordered-player-effect-chain.py" else
            ("Flanger", "Chorus") if example.name == "174-drum-effect-chain.py" else
             ("AirwindowsEchojam",)
             if example.name == "186-airwindows-echojam-on-chords.py" else
             ("Chorus", "Phaser", "Tremolo")
             if example.name == "187-effect-chain-control-sliders.py" else
            ("JPverb", "Greyhole") if example.parent == space_portable else
             ("LimutPingPong", "LimutDuck")
             if example.name == "195-limut-pingpong-duck.py" else
             ("LimutDisperser", "LimutFuzz")
             if example.name == "196-limut-disperser-fuzz.py" else
             ("LimutShifter", "LimutShimmer", "LimutGatedVerb")
             if example.name in ("197-limut-shifter-shimmer.py",
                                 "198-limut-gated-reverb.py") else
             ("LimutCompressor",)
             if example.name == "199-limut-compressor.py" else
             ("SCLOrkPhazerFX", "SCLOrkDecimatorFX",
              "SCLOrkCrossoverDistortionFX")
             if example.name == "201-sclork-insert-effects.py" else
             ("CustomTone",)
             if example.name == "202-custom-synth-live-controls.py" else
              ("Chorus", "Flanger", "Phaser")
              if example.name == "227-pbindfx-style-event-effects.py" else
              ("JPverb", "Greyhole", "LimutPingPong",
               "LimutShimmer", "LimutGatedVerb")
              if example.name == "228-pbindfx-style-delay-reverb-tails.py" else
             ("JazzKitKick", "JazzKitClosedHat", "JazzKitBrushTap",
              "JazzKitBrushSweep", "JazzKitRide", "JazzKitCrossStick",
              "JazzKitRackTom", "JazzKitFloorTom")
             if example.name == "214-jazz-brush-kit.py" else
             ("JazzKitKick", "JazzKitBrushTap", "JazzKitBrushSweep",
              "JazzKitClosedHat", "JazzKitOpenHat", "JazzKitRide",
              "JazzKitCrossStick", "JazzKitRackTom")
             if example.name == "217-jazz-kit-core-groove.py" else
             ("JazzKitKick", "JazzKitBrushKick", "JazzKitBrushTap",
              "JazzKitBrushSweep", "JazzKitClosedHat", "JazzKitOpenHat",
              "JazzKitRide", "JazzKitCrossStick")
             if example.name == "218-jazz-kit-kick-compare.py" else
            ("CustomPluck",) if example.name == "94-user-synthdef-loader.py" else
            ("AirwindowsTapeDelay2",)
            if example.name == "103-sample-player-airwindows-delay.py" else
            ("AirwindowsEchojam",)
            if example.name == "104-sample-player-airwindows-echojam.py" else
            ("AirwindowsTapeDelay2", "AirwindowsEchojam")
            if example.name == "105-airwindows-timevar-controls.py" else ())
        wrapper = api_tests["make_wrapper"](str(example), str(result), user_synth_names)
        exec(wrapper, {"__name__": "__main__"})
        parsed = subprocess.run([str(binary), str(result), str(len(user_synth_names))],
                                capture_output=True, text=True)
        assert parsed.returncode == 0, (
            example.name, parsed.returncode, parsed.stderr, result.read_text())
        if example.name == "106-diskin-streaming.py":
            assert "D:1:0:DiskIO/drum-loop.wav:1:0" in parsed.stdout, parsed.stdout
        if example.name == "107-vdiskin-variable-rate.py":
            assert "D:0:0:" in parsed.stdout and "D:2:0:DiskIO/drum-loop.wav:0.75:0" in parsed.stdout
        if example.name == "108-diskout-safe-recording.py":
            assert "D:3:0::1:8" in parsed.stdout, parsed.stdout
        if example.name == "72-var-sample-and-rate.py":
            assert "Q:v:0.0,1.0:4.0,4.0:v:1.0,-1.0:4.0,4.0" in parsed.stdout
        if example.name == "73-linvar-sample-and-rate.py":
            assert "Q:l:0.0,1.0:4.0,4.0:l:0.75,1.5:4.0,4.0" in parsed.stdout
        if example.name == "93-clock-swing.py":
            assert parsed.stdout.count("W:0.04") == 2, parsed.stdout
        if example.name == "136-variable-bpm.py":
            assert parsed.stdout.count("T:v:90.0,135.0,105.0:8.0,8.0,8.0") == 2
        if example.name == "103-sample-player-airwindows-delay.py":
            assert "A:0" in parsed.stdout and "A:-1" in parsed.stdout, parsed.stdout
        if example.name == "104-sample-player-airwindows-echojam.py":
            assert "A:0" in parsed.stdout and "A:-1" in parsed.stdout, parsed.stdout
        if example.name == "105-airwindows-timevar-controls.py":
            assert parsed.stdout.count("X:") == 2, parsed.stdout
            assert "v:0.22,0.68:2.0,2.0" in parsed.stdout, parsed.stdout
            assert "l:0.18,0.62:3.0,3.0" in parsed.stdout, parsed.stdout
        if example.name == "163-foxdot-shape-drive-effects.py":
            assert parsed.stdout.count("H:") == 3, parsed.stdout
            assert "H:0.25:0:-:v:0.0,0.35:4.0,4.0" in parsed.stdout, parsed.stdout
            assert "H:0:0:v:0.0,0.45,0.9:2.0,2.0,2.0:-" in parsed.stdout, parsed.stdout
            assert "H:0:0.1:-:l:0.1,0.8:4.0,4.0" in parsed.stdout, parsed.stdout
        if example.name == "164-foxdot-vibrato-slide-bend.py":
            assert "N:0.02:0:0:0:0.25:0.2:0:0:1" in parsed.stdout, parsed.stdout
            assert "N:0.5:0.55:0:0.35:0:0:0:0:1" in parsed.stdout, parsed.stdout
            assert "Y:-,-,-,v:0.35,0.5:4.0,4.0,-,-,-,-,-," in parsed.stdout, parsed.stdout
            assert "Y:v:0.02,0.04:4.0,4.0,-,-,-,-,-,-,-," in parsed.stdout, parsed.stdout
        if example.name == "165-foxdot-chop-spin.py":
            assert "N:0.5:0:0:0:0:0:4:0:1" in parsed.stdout, parsed.stdout
            assert "N:0.5:0:0:0:0:0:8:2:1" in parsed.stdout, parsed.stdout
            assert "Y:-,-,-,-,-,-,v:4.0,8.0:4.0,4.0,-,-," in parsed.stdout, parsed.stdout
            assert "Y:-,-,-,-,-,-,-,l:0.0,6.0:4.0,4.0,-," in parsed.stdout, parsed.stdout
        if example.name == "168-chorus-stereo-width.py":
            assert "A:0" in parsed.stdout and "B:0.35,0.151515" in parsed.stdout
        if example.name == "169-flanger-sweep.py":
            assert "A:0" in parsed.stdout and "B:0.2," in parsed.stdout
            assert "l:0.2,0.55:4.0,4.0" in parsed.stdout, parsed.stdout
            assert "v:0.017721518987" in parsed.stdout, parsed.stdout
        if example.name == "170-chorus-flanger-together.py":
            assert "A:0" in parsed.stdout and "A:1" in parsed.stdout
        if example.name == "171-phaser-sweep.py":
            assert "A:0" in parsed.stdout and "v:" in parsed.stdout
            assert "v:0.06666666666666667,0.16363636363636364,0.3696969696969697" in parsed.stdout, parsed.stdout
        if example.name == "172-tremolo-motion.py":
            assert parsed.stdout.count("A:0") == 2
            assert "l:0.0,0.25:4.0,4.0" in parsed.stdout, parsed.stdout
        if example.name == "173-ordered-player-effect-chain.py":
            assert "J:2:0[" in parsed.stdout and "]1[" in parsed.stdout, parsed.stdout
            assert "v:0.147869674185" in parsed.stdout, parsed.stdout
        if example.name == "174-drum-effect-chain.py":
            assert "J:2:0[" in parsed.stdout and "J:0:" in parsed.stdout, parsed.stdout
            assert "l:0.2,0.65:4.0,4.0" in parsed.stdout, parsed.stdout
        if example.name == "175-foxdot-cut-controls.py":
            assert "N:0.5:0:0:0:0:0:0:0:0.75" in parsed.stdout, parsed.stdout
            assert "N:0.5:0:0:0:0:0:0:0:0.35" in parsed.stdout, parsed.stdout
            assert "Y:-,-,-,-,-,-,-,-,v:0.8,0.35:1.0,1.0," in parsed.stdout, parsed.stdout
        if example.name == "177-pshift-bits-crush.py":
            assert "V:0:1:v:0.0,10.0,6.0:4.0,4.0,4.0:v:1.0,4.0,12.0:4.0,4.0,4.0" in parsed.stdout, parsed.stdout
            assert "v:1.0,4.0,12.0:4.0,4.0,4.0" in parsed.stdout, parsed.stdout
            assert "V:8:6:-:-" in parsed.stdout, parsed.stdout
            assert "v:0.0,7.0,12.0,-12.0:2.0,2.0,2.0,2.0" in parsed.stdout, parsed.stdout
            noteRows = [token for token in parsed.stdout.split()
                        if token.startswith("N:")]
            assert any(row.endswith(":12") for row in noteRows), parsed.stdout
            assert any(row.endswith(":-12") for row in noteRows), parsed.stdout
    for invalid in (
            "OK\t0\t0\t17\n",
            "OK\t0\t0\t1\nD\t9\t0\t1\t.5\t0\t0\t0\t78\n",
            "OK\t0\t0\t1\nD\t1\t8\t1\t.5\t0\t0\t0\t78\n",
            "OK\t0\t0\t1\nD\t1\t0\t-1\t.5\t0\t0\t0\t78\n",
            "OK\t0\t0\t1\nD\t3\t0\t1\t.5\t0\t3601\t0\t-\n",
            "OK\t0\t0\t1\nD\t1\t0\t1\t.5\t0\t0\t0\t00\n",
            "OK\t0\t0\t1\nD\t1\t0\t1\t.5\t0\t0\t0\txyz\n"):
        result.write_text(invalid)
        rejected = subprocess.run([str(binary), str(result), "0"],
                                  capture_output=True, text=True)
        assert rejected.returncode == 1, (invalid, rejected.stdout, rejected.stderr)
    reverb_example = space_portable / "167-space-reverb-timevars.py"
    exec(api_tests["make_wrapper"](str(reverb_example), str(result),
                                  ("JPverb", "Greyhole")),
         {"__name__": "__main__"})
    valid_reverbs = result.read_text()
    for bad in ("-0.1", "1.1", "inf", "nan"):
        malformed = re.sub(r"^(W\t0\t)[^\t]+", r"\g<1>" + bad,
                           valid_reverbs, count=1, flags=re.M)
        assert malformed != valid_reverbs
        result.write_text(malformed)
        rejected = subprocess.run([str(binary), str(result), "2"],
                                  capture_output=True, text=True)
        assert rejected.returncode == 1 and "player-effect controls" in rejected.stderr
    pitch_effect_example = plugin / "Portable" / "164-foxdot-vibrato-slide-bend.py"
    pitch_effect_wrapper = api_tests["make_wrapper"](
        str(pitch_effect_example), str(result), ())
    exec(pitch_effect_wrapper, {"__name__": "__main__"})
    valid_pitch_effects = result.read_text()
    invalid_vibdepth = re.sub(
        r"^(Y\t\d+\t)v:0\.02,0\.04:",
        r"\1v:11,0.04:", valid_pitch_effects,
        count=1, flags=re.M)
    assert invalid_vibdepth != valid_pitch_effects
    result.write_text(invalid_vibdepth)
    rejected_vibdepth = subprocess.run([str(binary), str(result), "0"],
                                       capture_output=True, text=True)
    assert rejected_vibdepth.returncode == 1
    assert "invalid note-effect time-var control" in rejected_vibdepth.stderr
    cut_example = plugin / "Portable" / "175-foxdot-cut-controls.py"
    cut_wrapper = api_tests["make_wrapper"](str(cut_example), str(result), ())
    exec(cut_wrapper, {"__name__": "__main__"})
    valid_cut = result.read_text()
    invalid_cut_event = re.sub(
        r"^(N\t(?:[^\t]*\t){15})0\.75\t",
        r"\g<1>1.2\t", valid_cut,
        count=1, flags=re.M)
    assert invalid_cut_event != valid_cut
    result.write_text(invalid_cut_event)
    rejected_cut_event = subprocess.run([str(binary), str(result), "0"],
                                        capture_output=True, text=True)
    assert rejected_cut_event.returncode == 1
    invalid_cut_var = re.sub(
        r"^(Y\t\d+\t-\t-\t-\t-\t-\t-\t-\t-\t)v:0\.8,0\.35:",
        r"\g<1>v:0.8,1.2:", valid_cut, count=1, flags=re.M)
    assert invalid_cut_var != valid_cut
    result.write_text(invalid_cut_var)
    rejected_cut_var = subprocess.run([str(binary), str(result), "0"],
                                      capture_output=True, text=True)
    assert rejected_cut_var.returncode == 1
    assert "invalid note-effect time-var control" in rejected_cut_var.stderr
    chop_spin_example = plugin / "Portable" / "165-foxdot-chop-spin.py"
    chop_spin_wrapper = api_tests["make_wrapper"](
        str(chop_spin_example), str(result))
    exec(chop_spin_wrapper, {"__name__": "__main__"})
    valid_chop_spin = result.read_text()
    invalid_chop = re.sub(
        r"^(Y\t\d+\t-\t-\t-\t-\t-\t-\t)v:4\.0,8\.0:",
        r"\g<1>v:33,8.0:", valid_chop_spin, count=1, flags=re.M)
    assert invalid_chop != valid_chop_spin
    result.write_text(invalid_chop)
    rejected_chop = subprocess.run([str(binary), str(result), "0"],
                                   capture_output=True, text=True)
    assert rejected_chop.returncode == 1
    invalid_spin = re.sub(
        r"^(Y\t\d+\t-\t-\t-\t-\t-\t-\t-\t)l:0\.0,6\.0:",
        r"\g<1>l:0.0,41:", valid_chop_spin, count=1, flags=re.M)
    assert invalid_spin != valid_chop_spin
    result.write_text(invalid_spin)
    rejected_spin = subprocess.run([str(binary), str(result), "0"],
                                   capture_output=True, text=True)
    assert rejected_spin.returncode == 1
    user_synth_example = plugin / "Portable" / "94-user-synthdef-loader.py"
    user_synth_wrapper = api_tests["make_wrapper"](
        str(user_synth_example), str(result), ("CustomPluck",))
    exec(user_synth_wrapper, {"__name__": "__main__"})
    rejected_user_synth = subprocess.run([str(binary), str(result), "0"],
                                         capture_output=True, text=True)
    assert rejected_user_synth.returncode == 1
    assert "synth" in rejected_user_synth.stderr.lower(), rejected_user_synth.stderr
    airwindows_example = plugin / "Portable" / "103-sample-player-airwindows-delay.py"
    airwindows_wrapper = api_tests["make_wrapper"](
        str(airwindows_example), str(result), ("AirwindowsTapeDelay2",))
    exec(airwindows_wrapper, {"__name__": "__main__"})
    valid_airwindows = result.read_text()
    bad_effect_index = re.sub(r"^A\t0\t0$", "A\t0\t4", valid_airwindows,
                              count=1, flags=re.M)
    assert bad_effect_index != valid_airwindows
    result.write_text(bad_effect_index)
    rejected_effect_index = subprocess.run([str(binary), str(result), "1"],
                                           capture_output=True, text=True)
    assert rejected_effect_index.returncode == 1
    assert "effect selection" in rejected_effect_index.stderr.lower()
    swing_example = plugin / "Portable" / "93-clock-swing.py"
    swing_wrapper = api_tests["make_wrapper"](str(swing_example), str(result))
    exec(swing_wrapper, {"__name__": "__main__"})
    valid_swing = result.read_text()
    for bad_value in ("inf", "0.245"):
        lines = valid_swing.splitlines()
        player_line = next(i for i, line in enumerate(lines) if line.startswith("P\t"))
        columns = lines[player_line].split("\t")
        columns[-1] = bad_value
        lines[player_line] = "\t".join(columns)
        result.write_text("\n".join(lines) + "\n")
        parsed = subprocess.run([str(binary), str(result), "0"],
                                capture_output=True, text=True)
        assert parsed.returncode == 1, (bad_value, parsed.stderr)
    effect_example = plugin / "Portable" / "163-foxdot-shape-drive-effects.py"
    effect_wrapper = api_tests["make_wrapper"](str(effect_example), str(result))
    exec(effect_wrapper, {"__name__": "__main__"})
    valid_effects = result.read_text()
    for field_index, bad_value in ((4, "5.0"), (8, "v:0.0,1.0:0.0,2.0")):
        lines = valid_effects.splitlines()
        modulation_line = next(i for i, line in enumerate(lines)
                               if line.startswith("G\t"))
        fields = lines[modulation_line].split("\t")
        fields[field_index] = bad_value
        lines[modulation_line] = "\t".join(fields)
        result.write_text("\n".join(lines) + "\n")
        rejected_effect = subprocess.run(
            [str(binary), str(result), "0"], capture_output=True, text=True)
        assert rejected_effect.returncode == 1, (
            bad_value, rejected_effect.stdout, rejected_effect.stderr)
    sample_example = plugin / "Portable" / "83-pbeat-pdur-rhythm-durations.py"
    sample_wrapper = api_tests["make_wrapper"](str(sample_example), str(result))
    exec(sample_wrapper, {"__name__": "__main__"})
    valid = result.read_text()
    high_tempo = re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                        r"\g<1>10000", valid, count=1, flags=re.M)
    assert high_tempo != valid
    result.write_text(high_tempo)
    parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
    assert parsed.returncode == 0, parsed.stderr
    low_tempo = re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                       r"\g<1>0.001", valid, count=1, flags=re.M)
    assert low_tempo != valid
    result.write_text(low_tempo)
    parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
    assert parsed.returncode == 0, parsed.stderr
    for bad_tempo in (
            re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                   r"\g<1>inf", valid, count=1, flags=re.M),
            re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                   r"\g<1>0", valid, count=1, flags=re.M),
            re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                   r"\g<1>0.0009", valid, count=1, flags=re.M)):
        assert bad_tempo != valid
        result.write_text(bad_tempo)
        parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
    for bad in (re.sub(r"^S\t0\t", "S\t9\t", valid, flags=re.M),
                re.sub(r"^S\t0\t[^\n]+", "S\t0\tnan\t0.5\t1\t0.5\t0.5\t0\t1", valid, flags=re.M),
                re.sub(r"^S\t[^\n]+\n", "", valid, flags=re.M),
                re.sub(r"^B\t[^\n]+\n", "", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\tnan\t0\t0\t1\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t0\t0\t0\t1\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t32\t0\t1\t0\t0\t1\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t1\t2\t0\t1\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t1\t0\t0.8\t0.2\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t1\t0\t0\t1\t2", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t1\t0\t0\t1\t1", valid, count=1, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
    for bad in (re.sub(r"^Q\t[^\n]+\n", "", valid, count=1, flags=re.M),
                re.sub(r"^Q\t[^\n]+", "Q\t9\tv:0,1:2,2\t-", valid, count=1, flags=re.M),
                re.sub(r"^Q\t[^\n]+", "Q\t0\tinvalid\t-", valid, count=1, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
    for bad in (re.sub(r"^T\t[^\n]+\n", "", valid, count=1, flags=re.M),
                re.sub(r"^T\t0\t[^\n]+", "T\t9\t-", valid, count=1, flags=re.M),
                re.sub(r"^T\t0\t[^\n]+", "T\t0\tinvalid", valid, count=1, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
        assert parsed.returncode == 1 and "Clock.bpm" in parsed.stderr, parsed.stderr

    fx_example = plugin / "Portable" / "105-airwindows-timevar-controls.py"
    fx_wrapper = api_tests["make_wrapper"](
        str(fx_example), str(result),
        ("AirwindowsTapeDelay2", "AirwindowsEchojam"))
    exec(fx_wrapper, {"__name__": "__main__"})
    valid_fx = result.read_text()
    assert valid_fx.count("\nX\t") == 3
    bad_fx_token = re.sub(r"^X\t0\t[^\n]+",
                          "X\t0\tinvalid\t-\t-\t-\t-\t-\t-",
                          valid_fx, count=1, flags=re.M)
    out_of_range_fx_token = re.sub(
        r"^X\t0\t[^\n]+",
        "X\t0\tv:0.2,1.1:2.0,2.0\t-\t-\t-\t-\t-\t-",
        valid_fx, count=1, flags=re.M)
    assert bad_fx_token != valid_fx
    for bad in (bad_fx_token, out_of_range_fx_token):
        assert bad != valid_fx
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result), "2"],
                                capture_output=True, text=True)
        assert parsed.returncode == 1 and "effect-control modulation" in parsed.stderr, parsed.stderr
    missing_fx_record = re.sub(r"^X\t[^\n]+\n", "", valid_fx,
                               count=1, flags=re.M)
    assert missing_fx_record != valid_fx
    result.write_text(missing_fx_record)
    parsed = subprocess.run([str(binary), str(result), "2"],
                            capture_output=True, text=True)
    assert parsed.returncode == 1 and "effect controls" in parsed.stderr, parsed.stderr

    safe_overlap_source = directory / "safe-overlap.py"
    safe_overlap_source.write_text(
        "p1 >> sine([(0, 1, 2, 3, 4), (5, 6, 7, 8)], dur=1, sus=0.5)\n")
    overlap_wrapper = api_tests["make_wrapper"](
        str(safe_overlap_source), str(result))
    exec(overlap_wrapper, {"__name__": "__main__"})
    overlap_lines = result.read_text().splitlines()
    note_indexes = [index for index, line in enumerate(overlap_lines)
                    if line.startswith("N\t")]
    assert len(note_indexes) == 9
    for index in note_indexes[:5]:
        fields = overlap_lines[index].split("\t")
        fields[2] = "1.5"
        overlap_lines[index] = "\t".join(fields)
    result.write_text("\n".join(overlap_lines) + "\n")
    parsed = subprocess.run([str(binary), str(result), "0"], capture_output=True, text=True)
    assert parsed.returncode == 1 and "exceeds 8-note polyphony" in parsed.stderr, parsed.stderr
    chainSource = directory / "chain.py"
    chainSource.write_text(api_tests["chainScript"])
    exec(api_tests["make_wrapper"](str(chainSource), str(result),
                                 api_tests["chainNames"]), {"__name__": "__main__"})
    validChain = result.read_text()
    chainLine = next(line for line in validChain.splitlines()
                     if line.startswith("J\t0\t"))
    fields = chainLine.split("\t")
    corruptions = []
    for index, value in ((1, "9"), (2, "4"), (3, "-1"), (3, "99"),
                         (4, "1.1"), (4, "nan"), (11, "bad"),
                         (11, "l:0,2:4,4")):
        badFields = fields.copy()
        badFields[index] = value
        corruptions.append(validChain.replace(chainLine, "\t".join(badFields)))
    corruptions += [
        validChain.replace(chainLine + "\n", ""),
        validChain.replace(chainLine, "\t".join(fields[:-1])),
        validChain.replace("A\t0\t0", "A\t0\t-1")]
    for corrupted in corruptions:
        result.write_text(corrupted)
        parsed = subprocess.run([str(binary), str(result), "2"],
                                capture_output=True, text=True)
        assert parsed.returncode == 1, (corrupted, parsed.stdout)
print(f"PASS: {len(examples)} example results accepted by compiled C++ parser; malformed synth/sample/effect-chain records rejected")
