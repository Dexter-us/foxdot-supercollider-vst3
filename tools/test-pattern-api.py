import ast
import builtins
import os
import re
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: test-pattern-api.py python_runtime.cpp Portable-directory')
source = open(sys.argv[1], encoding='utf-8').read()
start_marker = 'static constexpr char api[] = R"PY(' + chr(10)
end_marker = ')PY";'
start = source.find(start_marker)
if start < 0:
    raise SystemExit('Could not find the embedded Python API in python_runtime.cpp')
start += len(start_marker)
end = source.find(end_marker, start)
if end < 0:
    raise SystemExit('Could not find the end of the embedded Python API')
api = source[start:end]
checker_start_marker = 'static constexpr char compatibilityChecker[] = R"PY(' + chr(10)
checker_start = source.find(checker_start_marker)
if checker_start < 0:
    raise SystemExit('Could not find the embedded FoxDot compatibility checker')
checker_start += len(checker_start_marker)
checker_end = source.find(')PY";', checker_start)
if checker_end < 0:
    raise SystemExit('Could not find the end of the compatibility checker')
compatibility_checker = source[checker_start:checker_end]

# Reconstruct and compile the exact streamed Python wrapper used by C++.
wrapper_start = source.find('std::string makeEvaluationScript(')
wrapper_end = source.find('return script.str();', wrapper_start)
if wrapper_start < 0 or wrapper_end < 0:
    raise SystemExit('Could not find makeEvaluationScript in python_runtime.cpp')
wrapper_cpp = source[wrapper_start:wrapper_end]

def make_wrapper(source_path, result_path):
    parts = []
    placeholders = {'namespaceName': 'test_scope', 'api': api,
                    'sourcePath': source_path, 'resultPath': result_path}
    for token in re.findall(r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\))', wrapper_cpp):
        if token.startswith('"'):
            parts.append(ast.literal_eval(token))
        else:
            argument = token[len('pythonLiteral('):-1]
            if argument not in placeholders:
                raise SystemExit('Unknown generated-wrapper placeholder: ' + argument)
            parts.append(repr(placeholders[argument]))
    return ''.join(parts)

safe_names = ('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
              'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
              'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str',
              'sum', 'tuple', 'zip', 'ValueError', 'TypeError',
              'ZeroDivisionError', '__build_class__')
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_updates': {},
    '_dexter_default_touched': False, '__name__': 'dexter_test',
    '__builtins__': {name: getattr(builtins, name) for name in safe_names},
}
exec(api, namespace, namespace)
assert namespace['reverse']([1, 2, 3]) == [3, 2, 1]
assert namespace['rotate']([1, 2, 3], 1) == [3, 1, 2]
assert namespace['rotate']([1, 2, 3], -1) == [2, 3, 1]
assert namespace['repeat']([1, None], 3) == [1, None, 1, None, 1, None]
assert namespace['palindrome']([1, 2, 3, 4]) == [1, 2, 3, 4, 3, 2]
assert namespace['palindrome']([1, None, [3, 5]]) == [1, None, [3, 5], None]
assert namespace['stutter']([1, None, [3, 5]], 2) == [1, 1, None, None, [3, 5], [3, 5]]
assert namespace['interleave']([1, 2, 3], [None, 9], [4]) == [1, None, 4, 2, 9, 3]
assert namespace['transpose']([60, None, [64, 67]], 12) == [72, None, [76, 79]]
euclidean = namespace['euclid'](3, 8, 36, 1)
assert len(euclidean) == 8 and euclidean.count(36) == 3
fox_literal = namespace['P'][0, 2, None, (3, 5), 7]
assert fox_literal == [0, 2, None, (3, 5), 7]
assert fox_literal + 2 == [2, 4, None, (5, 7), 9]
assert 2 + namespace['P'][1, None, (3, 5)] == [3, None, (5, 7)]
assert namespace['P'][4, 8] - namespace['P'][1, 2, 3] == [3, 6, 1, 7, 2, 5]
assert 10 - namespace['P'][1, None, (2, 4)] == [9, None, (8, 6)]
pattern = namespace['P'][1, None, (3, 5), 4]
assert pattern.reverse() == [4, (3, 5), None, 1]
assert pattern.rotate(1) == [None, (3, 5), 4, 1]
assert pattern.rotate(-1) == [4, 1, None, (3, 5)]
assert namespace['P'][0, 1, 2, 3].stutter([1, 3]) == [0, 1, 1, 1, 2, 3, 3, 3]
assert namespace['P'][1, None, 3].repeat(2) == [1, None, 3, 1, None, 3]
assert namespace['P'][1, 2, 3].palindrome() == [1, 2, 3, 3, 2, 1]
try:
    namespace['P'][*range(129)].repeat(2)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('pattern repeat must enforce the 256-item cap')
left = namespace['P'][0, None, (2, 4)]
right = namespace['P'][5, 7]
assert left | right == [0, None, (2, 4), 5, 7]
assert [9] | namespace['P'][1, 2] == [9, 1, 2]
assert left & right == [(0, 5), 7, (2, 4, 5), (0, 7), 5, (2, 4, 7)]
assert namespace['P'][None] & namespace['P'][None] == [None]
try:
    namespace['P'][*range(129)] | namespace['P'][*range(129)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('P concatenation must enforce its size cap')
try:
    namespace['P'][*range(17)] & namespace['P'][*range(19)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('P zipping must enforce its expansion cap')
try:
    namespace['P'][(1, 2, 3, 4, 5),] & namespace['P'][(6, 7, 8, 9),]
except ValueError as error:
    assert '8 notes' in str(error)
else:
    raise AssertionError('zipped chords must stay within 8 notes')


try:
    namespace['P'][*range(17)] + namespace['P'][*range(19)]
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('expanded P patterns must stay within the event cap')
assert namespace['P'][2, None, (3, 4)] * 3 == [6, None, (9, 12)]
assert 2 * namespace['P'][1, None, (3, 4)] == [2, None, (6, 8)]
assert namespace['P'][8, 9] / namespace['P'][2, 3] == [4, 3]
assert 10 / namespace['P'][2, 4] == [5, 2.5]
assert namespace['P'][8, 9] // 2 == [4, 4]
assert namespace['P'][8, 9] % 3 == [2, 0]
assert namespace['P'][2, 3] ** 2 == [4, 9]
assert namespace['P'][2, 3] ^ 3 == [8, 27]
assert namespace['P'][8, 9] / namespace['P'][2, 0] == [4, 0]
assert namespace['P'][8, 9] % namespace['P'][3, 0] == [2, 0]
try:
    namespace['P'][2] ** 17
except ValueError as error:
    assert 'exponents are limited' in str(error)
else:
    raise AssertionError('P powers must enforce the exponent cap')

try:
    namespace['P'][(1, 2),] + namespace['P'][(3, 4, 5),]
except ValueError as error:
    assert 'matching sizes' in str(error)
else:
    raise AssertionError('mismatched chord groups must be rejected')
try:
    namespace['P'][slice(0, 8)]
except ValueError as error:
    assert 'finite item lists' in str(error)
else:
    raise AssertionError('P slice ranges must be rejected rather than guessed')
namespace['play'](namespace['transpose']([60, None, [62, 65]], 12), step=0.5)
assert len(namespace['_dexter_events']) == 3
namespace['_dexter_updates'].clear()
namespace['_dexter_default_touched'] = False
namespace['p1'] >> namespace['sequence']([60, None, 64], step=0.5, duration=0.4,
                                       synth='square', amp=0.6, pan=-0.25,
                                       cutoff=3200, echo=0.3, delay=0.2)
update = namespace['_dexter_updates'][0]
assert update[0] is False and len(update[3]) == 2
assert update[4] == 3 and update[5] == 0.6 and update[6] == -0.25
assert update[7] == 3200.0 and update[8] == 0.3 and update[9] == 0.2
namespace['p2'] >> namespace['sequence']([0, 2, 4], step=1.0, degrees=True)
assert set(namespace['_dexter_updates']) == {0, 1}
assert namespace['_dexter_updates'][1][3][1][2] == 64
namespace['p1'].stop()
assert namespace['_dexter_updates'][0][0] is True

def run_script(script_text):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper = make_wrapper(source_path, result_path)
        try:
            compile(wrapper, '<generated evaluation wrapper>', 'exec')
        except SyntaxError as error:
            raise SystemExit('Generated Python evaluation wrapper is invalid: ' + str(error))
        wrapper_globals = {'__name__': '__main__'}
        exec(wrapper, wrapper_globals, wrapper_globals)
        lines = open(result_path, encoding='utf-8').read().splitlines()
        assert lines[0].startswith('OK\t') and 1 <= int(lines[0].split('\t')[1]) <= 8, lines[0]
        return [line.split('\t') for line in lines if line.startswith('P\t')]

def run_example(name):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read())

def run_compatibility_check(script_text):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        report_path = os.path.join(directory, 'report.txt')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        namespace = {'_compat_source_path': source_path, '_compat_report_path': report_path}
        exec(compile(compatibility_checker, '<embedded compatibility checker>', 'exec'), namespace, namespace)
        return open(report_path, encoding='utf-8').read()

def test_embedded_synthdef():
    plugin_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
    repo_root = os.path.abspath(os.path.join(plugin_dir, '..', '..'))
    generator = os.path.join(repo_root, 'spikes', 'direct-world', 'testdata', 'generate_tone.py')
    header = os.path.join(plugin_dir, 'tone_synthdef.h')
    with tempfile.TemporaryDirectory() as directory:
        generated = os.path.join(directory, 'tone.scsyndef')
        subprocess.run([sys.executable, generator, generated], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        generated_bytes = open(generated, 'rb').read()
        header_text = open(header, encoding='utf-8').read()
        embedded_bytes = bytes(int(value, 16) for value in
                               re.findall(r'0x([0-9a-fA-F]{2})', header_text))
        assert generated_bytes == embedded_bytes, 'embedded SynthDef differs from its generator'
    for ugen_name in (b'\x06SinOsc', b'\x05LFTri', b'\x03Saw', b'\x05Pulse'):
        assert ugen_name in generated_bytes, 'missing SuperCollider oscillator ' + repr(ugen_name)

test_embedded_synthdef()

legacy = run_script('tempo(120)\nplay([60, 64], step=0.5, duration=0.4)\n')
assert len(legacy) == 1 and len(legacy[0]) == 12
fox_literal_rows = run_script('p1 >> saw(P[0, 2, None, (3, 5), 7], dur=0.5, oct=5, sus=0.4)\n')
assert len(fox_literal_rows) == 1 and fox_literal_rows[0][5] == '5', fox_literal_rows
assert legacy[0][5] == '2' and legacy[0][6] == '0'
assert legacy[0][7] == '1.0' and legacy[0][8] == '0.0'
assert legacy[0][10] == '0.0' and legacy[0][11] == '0.25'
example7 = run_example('07-independent-players.py')
assert len(example7) == 2 and all(len(row) == 12 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.25'
example9 = run_example('09-foxdot-core.py')
assert len(example9) == 2 and all(len(row) == 12 for row in example9)
foxSaw, foxPulse = example9
assert foxSaw[1] == '0' and foxSaw[3] == '96.0' and foxSaw[6] == '2'
assert abs(float(foxSaw[7]) - 0.7) < 1e-6 and abs(float(foxSaw[8]) + 0.35) < 1e-6
assert foxPulse[1] == '1' and foxPulse[3] == '96.0' and foxPulse[6] == '3'
assert abs(float(foxPulse[7]) - 0.25) < 1e-6 and abs(float(foxPulse[8]) - 0.35) < 1e-6
example10Rows = run_example('10-pattern-transformations.py')
assert len(example10Rows) == 2 and all(len(row) == 12 for row in example10Rows)
assert example10Rows[0][5] == '7' and example10Rows[0][6] == '2'
assert example10Rows[1][5] == '4' and example10Rows[1][6] == '3'
example11Rows = run_example('11-foxdot-pattern-literals.py')
assert len(example11Rows) == 2 and all(len(row) == 12 for row in example11Rows)
assert example11Rows[0][5] == '7' and example11Rows[1][5] == '2'
example12Rows = run_example('12-pattern-arithmetic.py')
assert len(example12Rows) == 2 and all(len(row) == 12 for row in example12Rows)
assert example12Rows[0][5] == '5' and example12Rows[1][5] == '5'
example13Rows = run_example('13-pattern-methods.py')
assert len(example13Rows) == 5 and all(len(row) == 12 for row in example13Rows)
assert [row[5] for row in example13Rows] == ['8', '5', '4', '4', '4']
example14Rows = run_example('14-pattern-composition.py')
assert len(example14Rows) == 2 and all(len(row) == 12 for row in example14Rows)
assert [row[5] for row in example14Rows] == ['5', '7']
example15Rows = run_example('15-pattern-arithmetic-operators.py')
assert len(example15Rows) == 4 and all(len(row) == 12 for row in example15Rows)
assert [row[5] for row in example15Rows] == ['4', '2', '4', '4']
example8 = run_example('08-synths-and-controls.py')
assert len(example8) == 2 and all(len(row) == 12 for row in example8)
p1, p2 = example8
assert p1[1] == '0' and p1[6] == '1' and abs(float(p1[7]) - 0.7) < 1e-6
assert abs(float(p1[8]) + 0.4) < 1e-6 and p1[9] == '5000.0'
assert p1[10] == '0.0' and p1[11] == '0.25'
assert p2[1] == '1' and p2[6] == '2' and abs(float(p2[7]) - 0.5) < 1e-6
assert abs(float(p2[8]) - 0.4) < 1e-6 and p2[9] == '1800.0'
assert p2[10] == '0.25' and p2[11] == '0.24'
supported_report = run_compatibility_check('Clock.bpm = 120\nScale.default.set("minor")\np1 >> saw([0, 2, 4], dur=0.5, oct=5, sus=0.4, amp=0.7, pan=-0.2)\np2 >> pulse([0, None, 3], dur=1, cutoff=3000)\np3 >> saw(P[0, None, 2], dur=0.5)\np1.stop()\n')
assert 'No unsupported constructs detected' in supported_report, supported_report
assert 'script was not executed' in supported_report, supported_report
unsupported_report = run_compatibility_check('from FoxDot import *\nClock.swing = 0.2\np9 >> bass([0, 2], dur=0.5)\np1 >> saw([0], dur=1, room=0.4)\nplay("x-o-")\nP["x-o"]\nP[0:8]\nP[0] | ["x-o"]\n')
for phrase in ('Line 1:', 'Line 2:', 'Line 3:', 'Line 4:', 'Line 5:', 'bass()', 'room', 'Sample-string', 'String sample patterns', 'slice-based generation', 'Pattern | and & do not accept sample strings'):
    assert phrase.lower() in unsupported_report.lower(), (phrase, unsupported_report)
syntax_report = run_compatibility_check('p1 >> saw([0])\nif :\n')
assert 'Python syntax error at column' in syntax_report and 'Line 2:' in syntax_report, syntax_report
for filename in os.listdir(sys.argv[2]):
    if filename.endswith('.py'):
        example_report = run_compatibility_check(open(os.path.join(sys.argv[2], filename), encoding='utf-8').read())
        assert 'No unsupported constructs detected' in example_report, (filename, example_report)
print('Smoke tests passed: embedded runtime, pattern API, static FoxDot compatibility checks, line diagnostics, and Examples 01-15.')
