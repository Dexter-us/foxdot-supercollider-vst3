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
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>
"""
cpp += source[parse_start:parse_end]
cpp += r"""
int main(int argc, char** argv) {
    if (argc != 2) return 3;
    PythonPattern pattern;
    std::string error;
    if (!parseResult(argv[1], pattern, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    for (const auto& player : pattern.players) {
        std::cout << player.synthIndex << ' ';
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
        wrapper = api_tests["make_wrapper"](str(example), str(result))
        exec(wrapper, {"__name__": "__main__"})
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 0, (example.name, parsed.stderr)
        if example.name == "72-var-sample-and-rate.py":
            assert "Q:v:0.0,1.0:4.0,4.0:v:1.0,-1.0:4.0,4.0" in parsed.stdout
        if example.name == "73-linvar-sample-and-rate.py":
            assert "Q:l:0.0,1.0:4.0,4.0:l:0.75,1.5:4.0,4.0" in parsed.stdout
    sample_example = plugin / "Portable" / "83-pbeat-pdur-rhythm-durations.py"
    sample_wrapper = api_tests["make_wrapper"](str(sample_example), str(result))
    exec(sample_wrapper, {"__name__": "__main__"})
    valid = result.read_text()
    high_tempo = re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                        r"\g<1>10000", valid, count=1, flags=re.M)
    assert high_tempo != valid
    result.write_text(high_tempo)
    parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
    assert parsed.returncode == 0, parsed.stderr
    low_tempo = re.sub(r"^(P\t[^\t]+\t[^\t]+\t)[^\t]+",
                       r"\g<1>0.001", valid, count=1, flags=re.M)
    assert low_tempo != valid
    result.write_text(low_tempo)
    parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
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
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
    for bad in (re.sub(r"^S\t0\t", "S\t9\t", valid, flags=re.M),
                re.sub(r"^S\t0\t[^\n]+", "S\t0\tnan\t0.5\t1\t0.5\t0.5\t0\t1", valid, flags=re.M),
                re.sub(r"^S\t[^\n]+\n", "", valid, flags=re.M),
                re.sub(r"^B\t[^\n]+\n", "", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\tnan\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t0\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t32\t0\t1\t0", valid, count=1, flags=re.M),
                re.sub(r"^B\t[^\n]+", "B\t120\t0\t1\t2", valid, count=1, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
    for bad in (re.sub(r"^Q\t[^\n]+\n", "", valid, count=1, flags=re.M),
                re.sub(r"^Q\t[^\n]+", "Q\t9\tv:0,1:2,2\t-", valid, count=1, flags=re.M),
                re.sub(r"^Q\t[^\n]+", "Q\t0\tinvalid\t-", valid, count=1, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout

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
    parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
    assert parsed.returncode == 1 and "exceeds 8-note polyphony" in parsed.stderr, parsed.stderr
print(f"PASS: {len(examples)} example results accepted by compiled C++ parser; malformed synth/sample records rejected")
