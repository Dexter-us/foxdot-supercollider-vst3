#!/usr/bin/env python3
"""Focused regression checks for the optional Airwindows Portable examples."""

from __future__ import annotations

import ast
from pathlib import Path
import re
import struct
import sys
import tempfile


if len(sys.argv) != 3:
    raise SystemExit("usage: test-examples.py python_runtime.cpp Portable-directory")

runtime_path = Path(sys.argv[1]).resolve()
portable_path = Path(sys.argv[2]).resolve()
source = runtime_path.read_text(encoding="utf-8")

api_marker = 'static constexpr char api[] = R"PY(' + chr(10)
api_start = source.find(api_marker)
api_end_marker = ')PY";'
if api_start < 0:
    raise SystemExit("Could not find the embedded Python API.")
api_start += len(api_marker)
api_end = source.find(api_end_marker, api_start)
api = source[api_start:api_end]

checker_marker = 'static constexpr char compatibilityChecker[] = R"PY(' + chr(10)
checker_start = source.find(checker_marker)
if checker_start < 0:
    raise SystemExit("Could not find the embedded compatibility checker.")
checker_start += len(checker_marker)
checker_end = source.find(')PY";', checker_start)
compatibility_checker = source[checker_start:checker_end]

wrapper_start = source.find("std::string makeEvaluationScript(")
wrapper_end = source.find("return script.str();", wrapper_start)
if wrapper_start < 0 or wrapper_end < 0:
    raise SystemExit("Could not find the embedded evaluation wrapper.")
wrapper_cpp = source[wrapper_start:wrapper_end]

def make_wrapper(source_path: str, result_path: str, synth_names: tuple[str, ...]) -> str:
    placeholders = {
        "namespaceName": "airwindows_test_scope",
        "api": api,
        "sourcePath": source_path,
        "resultPath": result_path,
        "userSynthNames": synth_names,
    }
    parts: list[str] = []
    tokens = re.findall(
        r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\)|pythonStringTuple\([^)]*\))',
        wrapper_cpp,
    )
    for token in tokens:
        if token.startswith('"'):
            parts.append(ast.literal_eval(token))
        elif token.startswith("pythonStringTuple("):
            parts.append(repr(placeholders["userSynthNames"]))
        else:
            argument = token[len("pythonLiteral("):-1]
            if argument not in placeholders:
                raise SystemExit(f"Unknown evaluation-wrapper placeholder: {argument}")
            parts.append(repr(placeholders[argument]))
    return "".join(parts)


def compatibility_report(script_text: str, synth_names: tuple[str, ...]) -> str:
    with tempfile.TemporaryDirectory(prefix="airwindows-check-") as temp:
        source_file = Path(temp) / "example.py"
        report_file = Path(temp) / "report.txt"
        source_file.write_text(script_text, encoding="utf-8")
        checker_globals = {
            "_compat_source_path": str(source_file),
            "_compat_report_path": str(report_file),
            "_USER_SYNTH_NAMES": synth_names,
        }
        exec(compile(compatibility_checker, "<embedded compatibility checker>", "exec"),
             checker_globals, checker_globals)
        return report_file.read_text(encoding="utf-8")


def synthdef_info(data: bytes) -> tuple[str, dict[str, int], set[str]]:
    if data[:4] != b"SCgf":
        raise AssertionError("Invalid SynthDef header.")
    offset = 4

    def read(fmt: str) -> int:
        nonlocal offset
        value = struct.unpack_from(fmt, data, offset)[0]
        offset += struct.calcsize(fmt)
        return value

    def read_string() -> str:
        nonlocal offset
        length = read(">B")
        value = data[offset:offset + length].decode("utf-8")
        offset += length
        return value

    version = read(">i")
    if version not in (1, 2):
        raise AssertionError(f"Unsupported SynthDef version {version}")
    count_format = ">i" if version == 2 else ">H"
    index_format = ">i" if version == 2 else ">h"
    graph_count = read(">H")
    if graph_count != 1:
        raise AssertionError(f"Expected one SynthDef, got {graph_count}")
    name = read_string()
    constant_count = read(count_format)
    offset += constant_count * 4
    parameter_count = read(count_format)
    offset += parameter_count * 4
    control_count = read(count_format)
    controls: dict[str, int] = {}
    for _ in range(control_count):
        control_name = read_string()
        controls[control_name] = read(count_format)
    ugen_names: set[str] = set()
    for _ in range(read(count_format)):
        ugen_names.add(read_string())
        read(">B")  # calculation rate
        input_count = read(count_format)
        output_count = read(count_format)
        read(">h")  # special index
        for _ in range(input_count):
            read(index_format)
            read(index_format)
        offset += output_count  # one byte per output rate
    if read(">H") != 0 or offset != len(data):
        raise AssertionError("Unexpected SynthDef variant or trailing data.")
    return name, controls, ugen_names


def evaluate(script_text: str, synth_names: tuple[str, ...]) -> tuple[list[list[str]], dict[int, list[float]]]:
    with tempfile.TemporaryDirectory(prefix="airwindows-eval-") as temp:
        source_file = Path(temp) / "example.py"
        result_file = Path(temp) / "result.txt"
        source_file.write_text(script_text, encoding="utf-8")
        wrapper = make_wrapper(str(source_file), str(result_file), synth_names)
        wrapper_globals = {"__name__": "__main__"}
        exec(compile(wrapper, "<embedded evaluation wrapper>", "exec"),
             wrapper_globals, wrapper_globals)
        lines = result_file.read_text(encoding="utf-8").splitlines()
        if not lines or not lines[0].startswith("OK\t"):
            raise AssertionError("\n".join(lines[:20]))
        players = [line.split("\t") for line in lines if line.startswith("P\t")]
        control_lines = [line.split("\t") for line in lines if line.startswith("S\t")]
        controls = {
            int(line[1]): [float(value) for value in line[2:]]
            for line in control_lines
        }
        return players, controls


examples = (
    ("95-airwindows-tape-delay2.py", ("AirwindowsTapeDelay2",), "TapeDelay2",
     (0.94, 0.28, 0.72, 0.28, 0.12, 0.0, 0.5)),
    ("96-airwindows-echojam.py", ("AirwindowsEchojam",), "Echojam",
     (0.29, 0.32, 0.48, 0.5, 0.5, 0.0, 0.62)),
)
for filename, synth_names, ugen_name, expected_controls in examples:
    synth_name = synth_names[0]
    example_path = portable_path / filename
    binary_path = portable_path / "SynthDefs" / f"{synth_name}.scsyndef"
    script_text = example_path.read_text(encoding="utf-8")
    if not binary_path.is_file():
        raise AssertionError(f"Missing or mismatched precompiled SynthDef: {binary_path}")
    binary_name_in_file, controls, ugen_names = synthdef_info(binary_path.read_bytes())
    expected_control_names = (
        "out freq rate sus unused4 unused5 wetbus wetgain wetcutoff trigger "
        "amp vib depth lforate lfowidth rq fmod blur cutoff atk rel beat_dur "
        "buf gate pan"
    ).split()
    if binary_name_in_file != synth_name:
        raise AssertionError(f"SynthDef name mismatch: {binary_name_in_file}")
    if controls != {name: index for index, name in enumerate(expected_control_names)}:
        raise AssertionError(f"{synth_name} does not match the VST's control-index ABI: {controls}")
    if ugen_name not in ugen_names:
        raise AssertionError(f"{synth_name} does not contain the expected {ugen_name} UGen.")
    report = compatibility_report(script_text, synth_names)
    if "No unsupported constructs detected" not in report:
        raise AssertionError(f"{filename} failed compatibility checking:\n{report}")
    rows, controls = evaluate(script_text, synth_names)
    if len(rows) != 1 or int(rows[0][6]) != 58:
        raise AssertionError(f"{filename} did not create the expected userSynth player: {rows}")
    if len(controls.get(0, ())) != len(expected_controls) or any(abs(actual - expected) > 1e-9
           for actual, expected in zip(controls.get(0, ()), expected_controls)):
        raise AssertionError(f"{filename} userSynth controls mismatch: {controls}")
    print(f"PASS {filename}: compatibility, embedded evaluation, SynthDef lookup and controls")

combined_path = portable_path / "97-airwindows-effects-together.py"
combined_script = combined_path.read_text(encoding="utf-8")
combined_names = ("AirwindowsTapeDelay2", "AirwindowsEchojam")
combined_report = compatibility_report(combined_script, combined_names)
if "No unsupported constructs detected" not in combined_report:
    raise AssertionError(f"Combined example failed compatibility checking:\n{combined_report}")
combined_rows, combined_controls = evaluate(combined_script, combined_names)
if len(combined_rows) != 2 or [int(row[6]) for row in combined_rows] != [58, 59]:
    raise AssertionError(f"Combined example did not start both independent effect players: {combined_rows}")
expected_by_player = {
    0: (0.94, 0.28, 0.72, 0.28, 0.12, 0.0, 0.5),
    1: (0.29, 0.32, 0.48, 0.5, 0.5, 0.0, 0.62),
}
for player, expected in expected_by_player.items():
    actual = combined_controls.get(player, ())
    if len(actual) != len(expected) or any(abs(value - target) > 1e-9
                                           for value, target in zip(actual, expected)):
        raise AssertionError(f"Combined example controls mismatch for player {player + 1}: {actual}")
print("PASS 97-airwindows-effects-together.py: both effects run on separate players concurrently")
