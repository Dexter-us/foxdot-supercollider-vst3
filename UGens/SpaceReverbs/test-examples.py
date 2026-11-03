#!/usr/bin/env python3
"""Exercise the actual embedded checker/wrapper and the insert SynthDef ABI."""
import ast
from pathlib import Path
import re
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PORTABLE = Path(__file__).resolve().parent / "Portable"
SOURCE = (ROOT / "spikes/vst3-callback/python_runtime.cpp").read_text()
NAMES = ("JPverb", "Greyhole")

def embedded(name):
    marker = 'static constexpr char ' + name + '[] = R"PY(\n'
    start = SOURCE.index(marker) + len(marker)
    return SOURCE[start:SOURCE.index(')PY";', start)]

wrapper_start = SOURCE.index("std::string makeEvaluationScript(")
wrapper_cpp = SOURCE[wrapper_start:SOURCE.index("return script.str();", wrapper_start)]

def wrapper(source, result, names=NAMES):
    values = dict(namespaceName="reverb_tests", sourcePath=str(source),
                  resultPath=str(result), userSynthNames=tuple(names),
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

def evaluate(text, names=NAMES, should_pass=True):
    with tempfile.TemporaryDirectory() as temp:
        source, result = Path(temp) / "input.py", Path(temp) / "output.txt"
        source.write_text(text)
        scope = {"__name__": "__main__"}
        exec(wrapper(source, result, names), scope, scope)
        output = result.read_text()
        assert output.startswith("OK\t") == should_pass, output
        return output, scope.get("reverb_tests", {})

def check(text, names=NAMES):
    with tempfile.TemporaryDirectory() as temp:
        source, result = Path(temp) / "input.py", Path(temp) / "report.txt"
        source.write_text(text)
        scope = dict(_compat_source_path=str(source),
                     _compat_report_path=str(result), _USER_SYNTH_NAMES=names)
        exec(embedded("compatibilityChecker"), scope, scope)
        return result.read_text()

def synthdef_info(path):
    data = path.read_bytes()
    offset = 4
    assert data[:4] == b"SCgf"
    def read(fmt):
        nonlocal offset
        result = struct.unpack_from(fmt, data, offset)[0]
        offset += struct.calcsize(fmt)
        return result
    def string():
        nonlocal offset
        size = read(">B")
        value = data[offset:offset + size].decode()
        offset += size
        return value
    assert read(">i") == 2 and read(">H") == 1
    name = string()
    constants = read(">i")
    offset += constants * 4
    defaults = [read(">f") for _ in range(read(">i"))]
    controls = {}
    for _ in range(read(">i")):
        key = string()
        controls[key] = read(">i")
    units = {}
    for _ in range(read(">i")):
        unit = string()
        rate, inputs, outputs, special = (
            read(">B"), read(">i"), read(">i"), read(">h"))
        offset += inputs * 8 + outputs
        units[unit] = (inputs, outputs)
    assert read(">H") == 0 and offset == len(data)
    return name, controls, defaults, units

for name, controls in (
        ("JPverb", "out inbus mix t60 damp size earlydiff moddepth modfreq"),
        ("Greyhole", "out inbus mix delaytime damp size diff feedback moddepth")):
    actual, indices, defaults, units = synthdef_info(
        PORTABLE / "SynthDefs" / (name + ".scsyndef"))
    assert actual == name
    assert indices == {key: i for i, key in enumerate(controls.split())}
    assert len(defaults) == 9
    assert units["In"] == (1, 2) and units["Out"] == (3, 0)
    assert name + "Raw" in units
    print("PASS", name, "stereo SynthDef ABI / original Raw UGen")

for path in sorted(PORTABLE.glob("*.py")):
    text = path.read_text()
    assert "No unsupported constructs detected" in check(text), check(text)
    output, scope = evaluate(text)
    selections = scope["_dexter_player_effect_indices"]
    assert selections[0] == 0 and selections[1] == 1
    assert all(0 <= value <= 1 for values in
               scope["_dexter_effect_control_values"].values() for value in values)
    assert scope["_dexter_synth_controls"][0] == (0, .5, 1, .5, .5, 0, 1)
    assert "W\t0\t" in output and "W\t1\t" in output
    print("PASS", path.name, "checker / wrapper / separate synth and effect controls")

for name in NAMES:
    assert "matching player-effect SynthDef" in check(
        'p1 >> pluck([0], effect="' + name + '")', ())
    output, _ = evaluate('p1 >> pluck([0], effect="' + name + '")',
                         names=(), should_pass=False)
    assert "not loaded" in output
    output, scope = evaluate(
        'p1 >> sine([0], vib=3, depth=0.3, effect="' + name +
        '", fx_mix=0.4)\np1.stop()\n')
    assert scope["_dexter_player_effect_indices"][0] == -1
    assert scope["_dexter_effect_control_variables"][0] == ("-",) * 7

# Every boundary is accepted; just-outside, NaN, infinity and bool are rejected.
_, scope = evaluate("p1 >> sine([0])")
specs = scope["_PLAYER_EFFECT_SPECS"]
for name, controls in specs.items():
    for keyword, slot, minimum, maximum, default in controls:
        for value in (minimum, maximum):
            text = ('p1 >> sine([0], effect=' + repr(name) + ', ' +
                    keyword + '=' + repr(value) + ')')
            evaluate(text, names=tuple(specs))
        for value in (minimum - .01, maximum + .01, float("inf"), float("nan"), True):
            literal = ("float('nan')" if value != value else
                       "float('inf')" if value == float("inf") else repr(value))
            text = ('p1 >> sine([0], effect=' + repr(name) + ', ' +
                    keyword + '=' + literal + ')')
            evaluate(text, names=tuple(specs), should_pass=False)

for effect, keyword, low, high in (("JPverb", "fx_t60", .1, 60),
                                 ("Greyhole", "fx_delay", .001, 1.45)):
    text = ('p1 >> pluck([0], effect=' + repr(effect) + ', ' + keyword +
            '=linvar([' + repr(low) + ',' + repr(high) + '], 4))')
    output, scope = evaluate(text)
    assert scope["_dexter_effect_control_variables"][0][1] == "l:0.0,1.0:4.0,4.0"
    evaluate(text.replace(',' + repr(high) + ']', ',' + repr(high + 1) + ']'),
             should_pass=False)

evaluate('p1 >> pluck([0], effect="Greyhole", fx_modfreq=4)', should_pass=False)
evaluate('p1 >> pluck([0], fx_mix=0.5)', should_pass=False)
evaluate('p1 >> pluck([0], effect="JPverb", fx_t61=2)', should_pass=False)
output, _ = evaluate('p1 >> userSynth("JPverb", [0])', should_pass=False)
assert 'is an insert, not a note source' in output
output, scope = evaluate(
    'p1 >> userSynth("AirwindowsTapeDelay2", [0], lforate=2)',
    names=("AirwindowsTapeDelay2",))
assert scope["_dexter_effect_control_values"][0][2] == 1
processor = (ROOT / "spikes/vst3-callback/processor.cpp").read_text()
for name in NAMES:
    branch = processor.split('else if (is' + name + ') {', 1)[1].split(
        '\n                }', 1)[0]
    arrays = re.findall(r'std::array<float, 7>\{\{([^}]+)\}\}', branch)
    ranges = [[float(v.strip().rstrip('f')) for v in values.split(",")]
              for values in arrays]
    minima, maxima = [0.] * 7, [1.] * 7
    for keyword, slot, minimum, maximum, default in specs[name]:
        minima[slot], maxima[slot] = minimum, maximum
    assert ranges == [minima, maxima], (name, ranges, minima, maxima)
assert 'updates[player].playerEffectControlValues, error)' in processor
assert 'hasSpaceReverb ? 65536u * 4u : 65536u' in processor
print("PASS all control endpoints, invalid controls, var/linvar normalization and stop reset")
