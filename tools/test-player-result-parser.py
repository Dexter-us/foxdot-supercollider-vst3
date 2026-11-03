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
source = source_path.read_text()
sys.argv = ["test-pattern-api.py", str(source_path), str(plugin / "Portable")]
api_tests = runpy.run_path(str(root / "tools/test-pattern-api.py"))
parse_start = source.index("bool parseResult(")
parse_end = source.index("} // namespace", parse_start)
cpp = r"""
#include "python_runtime.h"
#include "clock_swing.h"
#include "airwindows_control_smoothing.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
"""
cpp += "constexpr std::size_t kEmbeddedSynthCount = 58;\n"
cpp += source[parse_start:parse_end]
cpp += r"""
int main(int argc, char** argv) {
    if (argc != 3) return 3;
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
    if (std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.0)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.25)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.5) - 0.2) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 0.75) - 0.2) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(0.1, 120.0, 1.0)) > 1e-9 ||
        std::abs(foxDotClockSwingOffsetBeats(-0.1, 120.0, 0.5) + 0.2) > 1e-9 ||
        foxDotClockSwingOffsetBeats(0.0, 120.0, 0.5) != 0.0)
        return 4;
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
        std::cout << "W:" << player.swingSeconds << ' ';
        std::cout << "H:" << player.shape << ':' << player.drive << ':'
                  << player.shapeControlToken << ':'
                  << player.driveControlToken << ' ';
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
    }
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "parse.cpp", directory / "parse"
    unit.write_text(cpp)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(plugin), str(unit), "-o", str(binary)], check=True)
    examples = sorted((plugin / "Portable").glob("*.py"))
    for example in examples:
        result = directory / "result.out"
        user_synth_names = (
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
        assert parsed.returncode == 0, (example.name, parsed.stderr)
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
print(f"PASS: {len(examples)} example results accepted by compiled C++ parser; malformed synth/sample/effect-control records rejected")
