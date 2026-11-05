#!/usr/bin/env python3
"""Check LimutFX SynthDef ABI and examples with the embedded Python checker."""
import ast
import math
from pathlib import Path
import re
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE = (ROOT / "spikes/vst3-callback/python_runtime.cpp").read_text(
    encoding="utf-8", errors="replace")
NAMES = ("LimutPingPong", "LimutDuck")


def embedded(name):
    marker = "static constexpr char " + name + '[] = R"PY(\n'
    start = SOURCE.index(marker) + len(marker)
    return SOURCE[start:SOURCE.index(')PY";', start)]


wrapper_start = SOURCE.index("std::string makeEvaluationScript(")
wrapper_cpp = SOURCE[wrapper_start:SOURCE.index("return script.str();", wrapper_start)]


def wrapper(source, result):
    values = dict(namespaceName="limutfx_tests", sourcePath=str(source),
                  resultPath=str(result), userSynthNames=NAMES,
                  api=embedded("api"))
    parts = []
    for token in re.findall(
            r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\)|pythonStringTuple\([^)]*\))',
            wrapper_cpp):
        if token.startswith('"'):
            parts.append(ast.literal_eval(token))
        else:
            key = token[token.index("(") + 1:-1]
            parts.append(repr(values[key]))
    return "".join(parts)


def evaluate(text, should_pass=True):
    with tempfile.TemporaryDirectory() as temp:
        source, result = Path(temp) / "input.py", Path(temp) / "output.txt"
        source.write_text(text, encoding="utf-8")
        scope = {"__name__": "__main__"}
        exec(wrapper(source, result), scope, scope)
        output = result.read_text(encoding="utf-8")
        assert output.startswith("OK\t") == should_pass, output
        return output, scope.get("limutfx_tests", {})


def check(text, names=NAMES):
    with tempfile.TemporaryDirectory() as temp:
        source, result = Path(temp) / "input.py", Path(temp) / "report.txt"
        source.write_text(text, encoding="utf-8")
        scope = dict(_compat_source_path=str(source),
                     _compat_report_path=str(result), _USER_SYNTH_NAMES=names)
        exec(embedded("compatibilityChecker"), scope, scope)
        return result.read_text(encoding="utf-8")


def synthdef_info(path):
    data = path.read_bytes()
    offset = 4

    def read(fmt):
        nonlocal offset
        value = struct.unpack_from(fmt, data, offset)[0]
        offset += struct.calcsize(fmt)
        return value

    def read_string():
        nonlocal offset
        size = read(">B")
        value = data[offset:offset + size].decode()
        offset += size
        return value

    assert data[:4] == b"SCgf"
    assert read(">i") == 2 and read(">H") == 1
    name = read_string()
    constant_count = read(">i")
    offset += constant_count * 4
    defaults = [read(">f") for _ in range(read(">i"))]
    controls = {}
    for _ in range(read(">i")):
        key = read_string()
        controls[key] = read(">i")
    units = {}
    for _ in range(read(">i")):
        unit = read_string()
        rate, inputs, outputs, _ = read(">B"), read(">i"), read(">i"), read(">h")
        offset += inputs * 8 + outputs
        units[unit] = (inputs, outputs)
    assert read(">H") == 0 and offset == len(data)
    return name, controls, defaults, units


expected = {
    "LimutPingPong": (
        "out inbus mix delaytime feedback", "LimutPingPong", 4),
    "LimutDuck": (
        "out inbus mix period depth length attack", "LimutDuck", 6),
}
for name, (control_names, ugen_name, ugen_inputs) in expected.items():
    actual, controls, defaults, units = synthdef_info(
        HERE / "Portable/SynthDefs" / (name + ".scsyndef"))
    assert actual == name
    assert controls == {key: index for index, key in enumerate(control_names.split())}
    assert len(defaults) == len(controls)
    assert units["In"] == (1, 2) and units["Out"] == (3, 0)
    assert units[ugen_name] == (ugen_inputs, 2)
    print("PASS", name, "stereo SynthDef ABI")

for path in HERE.glob("Portable/*.py"):
    text = path.read_text(encoding="utf-8")
    report = check(text)
    assert "No unsupported constructs detected" in report, report
    output, scope = evaluate(text)
    assert output.count("W\t") >= 3, output
    assert scope["_dexter_player_effect_indices"] == {0: 0, 1: 1, 2: 0}
    for values in scope["_dexter_effect_control_values"].values():
        assert len(values) == 7 and all(
            math.isfinite(value) and 0.0 <= value <= 1.0 for value in values)
    print("PASS", path.name, "embedded checker and player-effect wrapper")

for name, control in (("LimutPingPong", "fx_time"),
                      ("LimutDuck", "fx_period")):
    text = f'p1 >> pluck([0], effect="{name}", {control}=var([0.25, 0.5], 4))'
    assert "No unsupported constructs detected" in check(text), check(text)
    assert "matching player-effect SynthDef" in check(text, ()), check(text, ())
    print("PASS", name, "time-variable controls and unloaded-def diagnostic")

bad = 'p1 >> pluck([0], effect="LimutDuck", fx_time=0.3)'
assert "is not a control on LimutDuck" in check(bad), check(bad)
