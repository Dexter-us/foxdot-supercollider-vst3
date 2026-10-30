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
    for (const auto& player : pattern.players)
        std::cout << player.synthIndex << ' ';
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "parse.cpp", directory / "parse"
    unit.write_text(cpp)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(plugin), str(unit), "-o", str(binary)], check=True)
    for example in sorted((plugin / "Portable").glob("*.py")):
        result = directory / "result.out"
        wrapper = api_tests["make_wrapper"](str(example), str(result))
        exec(wrapper, {"__name__": "__main__"})
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 0, (example.name, parsed.stderr)
    valid = result.read_text()
    for bad in (re.sub(r"^S\t0\t", "S\t9\t", valid, flags=re.M),
                re.sub(r"^S\t0\t[^\n]+", "S\t0\tnan\t0.5\t1\t0.5\t0.5\t0\t1", valid, flags=re.M),
                re.sub(r"^S\t[^\n]+\n", "", valid, flags=re.M)):
        assert bad != valid
        result.write_text(bad)
        parsed = subprocess.run([str(binary), str(result)], capture_output=True, text=True)
        assert parsed.returncode == 1, parsed.stdout
print("PASS: 69 example results accepted by compiled C++ parser; malformed synth records rejected")
