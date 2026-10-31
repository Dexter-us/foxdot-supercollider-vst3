import ast
import builtins
import os
import re
import struct
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: test-pattern-api.py python_runtime.cpp Portable-directory')
source = open(sys.argv[1], encoding='utf-8').read()
processor_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), 'processor.cpp')
processor_source = open(processor_path, encoding='utf-8').read()
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
              'ZeroDivisionError', '__build_class__', 'ord', 'chr')
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_updates': {}, '_dexter_mutes': {},
    '_dexter_shuffle_seed': 123456789,
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
stepVar = namespace['var']([0.2, 0.8], [2, 2])
linearVar = namespace['linvar']([-1, 1], 4)
assert stepVar.wire() == 'v:0.2,0.8:2.0,2.0'
assert linearVar.wire() == 'l:-1.0,1.0:4.0,4.0'
defaultVar = namespace['var']([1, 0])
assert defaultVar.durations == [4.0, 4.0]
assert stepVar.at(0) == 0.2 and stepVar.at(2) == 0.8
assert abs(linearVar.at(2)) < 1e-9 and linearVar.at(4) == 1.0
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
assert namespace['P']['x', ' '] | namespace['P']['o', '-'] == ['x', ' ', 'o', '-']
assert namespace['P']['x'] | 'o ' == ['x', 'o', ' ']
assert 'o ' | namespace['P']['x'] == ['o', ' ', 'x']
emptyPattern = namespace['_FoxDotFinitePattern']([])
assert emptyPattern | namespace['P']['x'] == ['x']
assert emptyPattern | namespace['P'][1] == [1]
try:
    namespace['P']['x'] | namespace['P'][1]
except TypeError as error:
    assert 'sample symbols or space-rests on both sides' in str(error)
else:
    raise AssertionError('sample/numeric P concatenation should be rejected')
assert left & right == [(0, 5), 7, (2, 4, 5), (0, 7), 5, (2, 4, 7)]
assert namespace['P'][None] & namespace['P'][None] == [None]
try:
    namespace['P']['x'] & namespace['P']['o']
except TypeError as error:
    assert 'numeric notes' in str(error)
else:
    raise AssertionError('sample-symbol P zipping should remain unsupported')
assert namespace['PAlt'](namespace['P']['x', ' '], namespace['P']['o', '-']) == [
    'x', 'o', ' ', '-']
assert namespace['PAlt'](
    namespace['P']['x', ' '], namespace['P']['o', '-', ' ']) == [
        'x', 'o', ' ', '-', 'x', ' ', ' ', 'o', 'x', '-', ' ', ' ']
assert namespace['PAlt'](
    namespace['P']['x'], namespace['P']['o'], namespace['P']['-']) == ['x', 'o', '-']
assert namespace['PStretch'](namespace['P']['x', ' '], 5) == [
    'x', ' ', 'x', ' ', 'x']
assert namespace['PStretch']('xo', 0) == []
assert namespace['PStutter'](namespace['P']['x', 'o']) == [
    'x', 'x', 'o', 'o']
assert namespace['PStutter'](
    namespace['P']['x', 'o'], namespace['P'][1, 3, 2]) == [
        'x', 'o', 'o', 'o', 'x', 'x', 'o', 'x', 'x', 'x', 'o', 'o']
assert namespace['PStutter'](
    namespace['P']['x', 'o', ' '], namespace['P'][1, 2]) == [
        'x', 'o', 'o', ' ', 'x', 'x', 'o', ' ', ' ']
shufSource = namespace['P']['x', 'o', 'x', ' ', '-']
shufBefore = list(shufSource)
shufResult = namespace['PShuf'](shufSource)
assert sorted(shufResult) == sorted(shufBefore) and list(shufSource) == shufBefore
assert list(shufResult) == [' ', 'x', '-', 'x', 'o']
shufSecondResult = namespace['PShuf'](shufSource)
assert list(shufSecondResult) == ['x', 'x', 'o', ' ', '-']
shufStringResult = namespace['PShuf']('x o-')
assert len(shufStringResult) == 4 and sorted(shufStringResult) == [' ', '-', 'o', 'x']
assert namespace['PJoin']([
    namespace['P']['x', ' '], namespace['P']['o', '-']]) == ['x', ' ', 'o', '-']
assert namespace['PJoin']([
    namespace['P']['x'], [], namespace['P']['o']]) == ['x', 'o']
assert namespace['PJoin'](namespace['P']['x', 'o']) == ['x', 'o']
assert len(namespace['PJoin'](['x' * 128, 'o' * 128])) == 256
assert namespace['PAlt'](namespace['P'][0, 2], namespace['P'][4, 6, 7]) == [
    0, 4, 2, 6, 0, 7, 2, 4, 0, 6, 2, 7]
assert namespace['PAlt'](
    namespace['P'][0, (2, 4), None], namespace['P'][7, 5, 3]) == [
        0, 7, (2, 4), 5, None, 3]
assert namespace['PStretch'](namespace['P'][0, (2, 4), None], 7) == [
    0, (2, 4), None, 0, (2, 4), None, 0]
assert namespace['PStutter'](
    namespace['P'][0, 2, 4], namespace['P'][1, 3]) == [
        0, 2, 2, 2, 4, 0, 0, 0, 2, 4, 4, 4]
numericShuffleSource = namespace['P'][0, (2, 4), None, 7, 2]
numericShuffleBefore = list(numericShuffleSource)
numericShuffle = namespace['PShuf'](numericShuffleSource)
assert sorted(map(repr, numericShuffle)) == sorted(map(repr, numericShuffleBefore))
assert list(numericShuffleSource) == numericShuffleBefore
assert namespace['PJoin']([
    namespace['P'][0, (2, 4)], namespace['P'][None, 7]]) == [
        0, (2, 4), None, 7]
assert len(namespace['PJoin']([
    namespace['P'][*range(128)], namespace['P'][*range(128, 256)] ])) == 256
for badJoin in (lambda: namespace['PJoin']([namespace['P'][0, 1], namespace['P']['x']]),
                lambda: namespace['PJoin'](['x' * 128, 'o' * 129]),
                lambda: namespace['PJoin']([[0] * 128, [1] * 129]),
                lambda: namespace['PJoin'](['x'] * 257),
                lambda: namespace['PJoin'](5)):
    try:
        badJoin()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('PJoin must reject unsupported or oversized sample patterns')
assert namespace['PBeat']('x xxx x') == [1.0, 0.5, 0.5, 1.0, 0.5]
assert namespace['PBeat']('x xx', 1, 0.25) == [0.25, 0.25, 0.5]
assert namespace['PBeat']('x xxx x', 6) == [1.0, 0.5, 0.5, 1.0, 0.5]
assert namespace['PBeat']('x xxx x', -1) == [0.5, 1.0, 0.5, 0.5, 1.0]
assert namespace['PBeat']('x x', dur=namespace['P'][0.25, 0.5]) == [0.5, 0.5]
assert namespace['PBeat']('') == [0.5]
assert namespace['PDur'](3, 8) == [0.75, 0.75, 0.5]
assert namespace['PDur'](5, 16) == [0.75, 0.75, 0.75, 0.75, 1.0]
assert namespace['PDur'](0, 8) == [2.0]
assert namespace['PDur'](3, 2) == [0.125, 0.125, 0.25]
assert namespace['PDur'](3, 8, 1, 0.25) == [0.75, 0.5, 0.75]
assert namespace['PDur'](3, 8, 4) == [0.75, 0.75, 0.5]
assert namespace['PDelay'](3, 8) == [0.0, 0.75, 1.5]
assert namespace['PDelay'](5, 16) == [0.0, 0.75, 1.5, 2.25, 3.0]
assert namespace['PDelay'](3, 8, 1, 0.25) == [0.0, 0.75, 1.25]
assert namespace['PStrum']() == [
    1.0, 0.5, 0.5, 0.5, 0.5, 0.5, 1.5, 0.5, 0.5, 0.5, 0.5, 1.0]
assert namespace['PStrum'](0) == [1.0, 0.5, 1.5, 1.0]
assert len(namespace['PStrum'](15)) == 34
assert [round(value, 6) for value in namespace['PQuicken']()] == [
    0.0, 0.166667, 0.333333, 0.388889, 0.444444, 0.462963,
    0.481481, 0.487654, 0.493827, 0.495885, 0.497942, 0.498628]
assert [round(value, 6) for value in namespace['PQuicken'](0.35, 2, 4)] == [
    0.0, 0.175, 0.2625, 0.30625]
assert len(namespace['PQuicken'](0.5, 17, 16)) == 256
assert namespace['PZip']([0, 1, 2], [3, 4]) == [
    (0, 3), (1, 4), (2, 3), (0, 4), (1, 3), (2, 4)]
assert namespace['PZip'](
    namespace['P'][0, 1], namespace['P'][3, 4],
    namespace['P'][5, 6, 7]) == [
        (0, 3, 5), (1, 4, 6), (0, 3, 7),
        (1, 4, 5), (0, 3, 6), (1, 4, 7)]
assert namespace['PZip'](2, [4, 5]) == [(2, 4), (2, 5)]
assert namespace['PZip']([None, 1], [3, 4]) == [
    (None, 3), (1, 4)]
assert namespace['PZip2']([0, 1, 2], [1, 2], rule=lambda a, b: a != b) == [
    (0, 1), (1, 2), (2, 1), (0, 2)]
assert namespace['PZip2']([0, 1], [3, 4]) == [(0, 3), (1, 4)]
assert namespace['PZip2']([1], [1], rule=lambda a, b: False) == []
assert namespace['PRhythm']([2, (3, 8)]) == [2.0, (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([(3, 8)]) == [0.75, 0.75, 0.5]
assert namespace['PRhythm']([2, (3, 8, 1, 0.25)]) == [
    2.0, (2.0, 0.75, 1.25)]
assert namespace['PRhythm'](namespace['P'][1, (3, 8)]) == [
    1.0, (2.0, 0.75, 1.5)]
for badRhythm in (
        lambda: namespace['PBeat'](123),
        lambda: namespace['PBeat']('x' * 257),
        lambda: namespace['PBeat']('x', dur=0),
        lambda: namespace['PBeat']('x', dur=namespace['P'][0.25, 0]),
        lambda: namespace['PDur'](3.0, 8),
        lambda: namespace['PDur'](1, 0),
        lambda: namespace['PDur'](-1, 8),
        lambda: namespace['PDur'](256, 129),
        lambda: namespace['PDur'](0, 256, dur=17),
        lambda: namespace['PDelay'](300, 128),
        lambda: namespace['PStrum'](-1),
        lambda: namespace['PStrum'](16),
        lambda: namespace['PStrum'](4.0),
        lambda: namespace['PQuicken'](0),
        lambda: namespace['PQuicken'](9),
        lambda: namespace['PQuicken'](0.5, 1, 4),
        lambda: namespace['PQuicken'](0.5, 17, 17),
        lambda: namespace['PQuicken'](0.5, 3.0, 4),
        lambda: namespace['PZip']([1]),
        lambda: namespace['PZip']([], [1]),
        lambda: namespace['PZip']([1] * 16, [2] * 17),
        lambda: namespace['PZip']([1] * 257, [2]),
        lambda: namespace['PZip']([1], 'xo'),
        lambda: namespace['PZip']([1], [(2, 3)]),
        lambda: namespace['PZip2']([1] * 16, [2] * 17),
        lambda: namespace['PZip2']([1], [2], rule=0),
        lambda: namespace['PRhythm']([]),
        lambda: namespace['PRhythm']([0]),
        lambda: namespace['PRhythm']([True]),
        lambda: namespace['PRhythm']([1, (3,)]),
        lambda: namespace['PRhythm']([1, (3.0, 8)]),
        lambda: namespace['PRhythm']([1, (3, 64)]),
        lambda: namespace['PRhythm']([[1, 2]]),
        lambda: namespace['PRhythm']([1] * 257)):
    try:
        badRhythm()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('rhythm helpers must reject invalid or unbounded inputs')
assert namespace['PStretch'](namespace['P']['x', 'o'], 256) == (
    list(namespace['P']['x', 'o']) * 128)
assert len(namespace['PStutter'](['x'] * 16, 16)) == 256
for badCall in (
        lambda: namespace['PStretch'](namespace['P'][0, 'x'], 4),
        lambda: namespace['PStutter'](namespace['P'][0, 'x']),
        lambda: namespace['PShuf'](namespace['P'][0, 'x']),
        lambda: namespace['PStretch'](namespace['P']['x'], 257),
        lambda: namespace['PStretch']([0], 257),
        lambda: namespace['PShuf']('x' * 257),
        lambda: namespace['PShuf']([0] * 257),
        lambda: namespace['PStutter'](['x'] * 17, 16),
        lambda: namespace['PStutter']([0] * 17, 16),
        lambda: namespace['PStutter'](namespace['P']['x'], namespace['P'][1, 1.5]),
        lambda: namespace['PStutter'](namespace['P']['x'], [1] * 257),
        lambda: namespace['PStutter'](
            namespace['P'][*range(17)], namespace['P'][*([1] * 19)])):
    try:
        badCall()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('PStretch/PStutter must reject invalid or oversized inputs')
for badPatterns in ((namespace['P'][0, 1], namespace['P']['x']),
                    (namespace['P']['x'], namespace['P'][1]),
                    (namespace['_FoxDotFinitePattern']([]), namespace['P']['x'])):
    try:
        namespace['PAlt'](*badPatterns)
    except (TypeError, ValueError) as error:
        assert ('patterns cannot be empty' in str(error) or
                'cannot mix numeric note patterns with sample-symbol patterns' in str(error))
    else:
        raise AssertionError('PAlt must reject empty or mixed-type patterns')
try:
    namespace['PAlt'](['x'] * 16, ['o'] * 17)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('PAlt must enforce the 256-symbol cap')
try:
    namespace['PAlt']([0] * 16, [1] * 17)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('numeric PAlt must enforce the 256-item cap')
try:
    namespace['PAlt']('x' * 257, namespace['P']['o'])
except ValueError as error:
    assert 'input patterns are limited to 256 items' in str(error)
else:
    raise AssertionError('PAlt must enforce the per-input limit')
try:
    namespace['P'][*range(129)] | namespace['P'][*range(129)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('P concatenation must enforce its size cap')
try:
    ['x'] * 256 | namespace['P']['o']
except ValueError as error:
    assert '256 items' in str(error)
else:
    raise AssertionError('sample P concatenation must enforce its size cap')
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
assert namespace['P'][slice(0, 8)] == list(range(8))
assert namespace['P'][slice(0, 8, 2)] == [0, 2, 4, 6]
assert namespace['P'][slice(8, 0, -2)] == [8, 6, 4, 2]
assert namespace['P'][slice(0, 8)][slice(2, 6)] + 1 == [3, 4, 5, 6]
try:
    namespace['P'][slice(0, None)]
except ValueError as error:
    assert 'finite stop' in str(error)
else:
    raise AssertionError('P generated ranges must require a finite stop')
try:
    namespace['P'][slice(0, 8, 0)]
except ValueError as error:
    assert 'cannot be zero' in str(error)
else:
    raise AssertionError('P generated ranges must reject a zero step')
try:
    namespace['P'][slice(0, 257)]
except ValueError as error:
    assert '256 generated items' in str(error)
else:
    raise AssertionError('P generated ranges must enforce the 256-item cap')
namespace['play'](namespace['transpose']([60, None, [62, 65]], 12), step=0.5)
assert len(namespace['_dexter_events']) == 3
namespace['_dexter_updates'].clear()
namespace['_dexter_default_touched'] = False
namespace['p1'] >> namespace['sequence']([60, None, 64], step=0.5, duration=0.4,
                                       synth='square', amp=0.6, pan=-0.25,
                                       cutoff=3200, echo=0.3, delay=0.2,
                                       reverb='gverb', room=0.8, mix=0.4,
                                       damp=0.25, revtime=4.0, spread=22)
update = namespace['_dexter_updates'][0]
assert update[0] is False and len(update[3]) == 2
assert update[4] == 3 and update[5] == 0.6 and update[6] == -0.25
assert abs(update[7] - 3200.0) < 1e-9 and abs(update[8] - 0.3) < 1e-9 and abs(update[9]) < 1e-9, repr(update[7:10])
assert abs(update[3][0][6] - 0.2) < 1e-9
assert update[19:25] == (3, 0.4, 0.8, 0.25, 4.0, 22.0)
assert update[10] is False and update[11] is False
assert update[12] == 0.01 and update[13] == 0.04
namespace['p2'] >> namespace['sequence']([0, 2, 4], step=1.0, degrees=True)
assert set(namespace['_dexter_updates']) == {0, 1}
assert namespace['_dexter_updates'][1][3][1][2] == 64
namespace['p1'].stop()
assert namespace['_dexter_updates'][0][0] is True

def run_script(script_text, include_events=False, include_effects=False, include_filters=False, include_decay=False, include_mutes=False, include_modulations=False, include_samples=False, include_sample_controls=False):
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
        assert (lines[0].startswith('OK\t') and
                0 <= int(lines[0].split('\t')[1]) <= 8), '\n'.join(lines[:20])
        player_rows = [line.split('\t') for line in lines if line.startswith('P\t')]
        event_rows = [line.split('\t') for line in lines if line.startswith('N\t')]
        effect_rows = [line.split('\t') for line in lines if line.startswith('R\t')]
        filter_rows = [line.split('\t') for line in lines if line.startswith('F\t')]
        modulation_rows = [line.split('\t') for line in lines if line.startswith('G\t')]
        decay_rows = [line.split('\t') for line in lines if line.startswith('E\t')]
        mute_rows = [line.split('\t') for line in lines if line.startswith('M\t')]
        if include_sample_controls:
            return (player_rows, [line.split('\t') for line in lines if line.startswith('B\t')],
                    [line.split('\t') for line in lines if line.startswith('Q\t')])
        if include_samples and include_events:
            return (player_rows, [line.split('\t') for line in lines if line.startswith('B\t')],
                    event_rows)
        if include_samples:
            return player_rows, [line.split('\t') for line in lines if line.startswith('B\t')]
        if include_mutes:
            return player_rows, mute_rows
        if include_filters:
            return player_rows, filter_rows
        if include_modulations:
            return player_rows, modulation_rows
        if include_decay:
            return player_rows, decay_rows
        if include_effects:
            return player_rows, effect_rows
        return (player_rows, event_rows) if include_events else player_rows

newSynthRows = run_script('p1 >> bass([0, 3, 5], dur=0.5)\np2 >> varsaw([0, 4], dur=1, rate=0.25)\np3 >> varsaw([0])\np4 >> dirt([0, 3])\np5 >> jbass([0, 5])\np6 >> noise([0, 2])\n')
assert len(newSynthRows) == 6
assert [int(row[6]) for row in newSynthRows] == [4, 5, 5, 6, 7, 8]
assert [float(row[21]) for row in newSynthRows] == [8.0, 0.25, 0.0, 0.0, 0.0, 0.0]
newFoxDotBatch = run_script('p1 >> charm([0], rate=0.5)\np2 >> spark([0])\np3 >> blip([0], rate=8)\n')
assert [int(row[6]) for row in newFoxDotBatch] == [22, 23, 24]
assert [float(row[21]) for row in newFoxDotBatch] == [0.5, 0.0, 8.0]
resonatorSynthRows = run_script('p1 >> bell([0], rate=1)\np2 >> gong([0])\np3 >> marimba([0])\np4 >> pluck([0])\np5 >> twang([0], rate=1)\np6 >> donk([0])\np7 >> squish([0], rate=1)\n')
assert len(resonatorSynthRows) == 7
assert [int(row[6]) for row in resonatorSynthRows] == [15, 16, 17, 18, 19, 20, 21]
assert [float(row[21]) for row in resonatorSynthRows] == [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0]

def run_error_script(script_text):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper_globals = {'__name__': '__main__'}
        exec(make_wrapper(source_path, result_path), wrapper_globals, wrapper_globals)
        return open(result_path, encoding='utf-8').read()

def run_example(name):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read())

def assert_player_polyphony_ok(player_row, event_rows):
    length = float(player_row[4])
    boundaries = []
    active = 0
    for event in event_rows:
        start = (float(event[1]) + float(event[7])) % length
        duration = float(event[2])
        full_cycles = int(duration // length)
        active += full_cycles
        remainder = duration - full_cycles * length
        if remainder <= 1e-9:
            continue
        end = start + remainder
        if end <= length:
            boundaries.extend(((start, 1), (end, -1)))
        else:
            boundaries.extend(((0.0, 1), (end - length, -1),
                               (start, 1), (length, -1)))
    assert active <= 8, ('player exceeds eight-note polyphony', active)
    for beat, delta in sorted(boundaries, key=lambda item: (item[0], item[1])):
        active += delta
        assert 0 <= active <= 8, ('player exceeds eight-note polyphony', beat, active)

initial_live_run = run_script('p1 >> saw([0, 2, 4], dur=0.5)\np2 >> pulse([0, None, 3], dur=0.25)\n')
assert [row[1] for row in initial_live_run] == ['0', '1']
edit_one_player = run_script('p1 >> saw([0, 4, 2], dur=0.5)\n')
assert [row[1] for row in edit_one_player] == ['0']
stop_one_player = run_script('p2.stop()\n')
assert [row[1] for row in stop_one_player] == ['1'] and stop_one_player[0][2] == '1'

def run_compatibility_check(script_text):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        report_path = os.path.join(directory, 'report.txt')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        namespace = {'_compat_source_path': source_path, '_compat_report_path': report_path}
        exec(compile(compatibility_checker, '<embedded compatibility checker>', 'exec'), namespace, namespace)
        return open(report_path, encoding='utf-8').read()

def read_synthdef_manifest(data):
    offset = 0

    def read(fmt):
        nonlocal offset
        values = struct.unpack_from(fmt, data, offset)
        offset += struct.calcsize(fmt)
        return values[0] if len(values) == 1 else values

    def read_string():
        nonlocal offset
        length = read('>B')
        value = data[offset:offset + length].decode('utf-8')
        offset += length
        return value

    assert data[:4] == b'SCgf'
    offset = 4
    version = read('>i')
    assert version in (1, 2)
    count_format = '>i' if version == 2 else '>H'
    index_format = '>i' if version == 2 else '>h'
    graph_count = read('>H')
    graphs = {}
    for _ in range(graph_count):
        name = read_string()
        constant_count = read(count_format)
        offset += constant_count * 4
        parameter_count = read(count_format)
        offset += parameter_count * 4
        parameter_name_count = read(count_format)
        parameter_names = {}
        for _ in range(parameter_name_count):
            parameter_name = read_string()
            parameter_names[parameter_name] = read(count_format)
        if version == 2:
            expected_controls = (
                'out freq rate sus unused4 unused5 wetbus wetgain wetcutoff trigger '
                'amp vib depth lforate lfowidth rq fmod blur cutoff atk rel beat_dur'
            ).split()
            if name == 'DexterSample':
                expected_controls.append('buf')
            assert parameter_names == {key: index for index, key in enumerate(expected_controls)}, (
                name, 'C++ control-number ABI mismatch', parameter_names)
        ugen_count = read(count_format)
        ugens = []
        for _ in range(ugen_count):
            ugen_name = read_string()
            rate = read('>B')
            input_count = read(count_format)
            output_count = read(count_format)
            read('>h')
            for _ in range(input_count):
                source_index = read(index_format)
                read(index_format)
                assert source_index >= -1 and (source_index == -1 or source_index < len(ugens)), (
                    'SynthDef UGen input refers to an invalid or forward graph node: ' +
                    name + '/' + ugen_name)
            for _ in range(output_count):
                read('>B')
            ugens.append((ugen_name, input_count, rate))
        assert read('>H') == 0, 'unexpected SynthDef variants'
        graphs[name] = ugens
    assert offset == len(data), 'SynthDef parser did not consume the complete file'
    return version, graphs


def test_embedded_synthdef():
    plugin_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
    repo_root = os.path.abspath(os.path.join(plugin_dir, '..', '..'))
    cmake_text = open(os.path.join(plugin_dir, 'CMakeLists.txt'), encoding='utf-8').read()
    version = re.search(r'project\([^\n]*\bVERSION\s+([0-9.]+)', cmake_text).group(1)
    factory_text = processor_source.split('BEGIN_FACTORY_DEF', 1)[1]
    assert '"' + version + '"' in factory_text, 'VST factory version differs from CMake/moduleinfo'
    editor_text = open(os.path.join(plugin_dir, 'win32_code_editor.cpp'), encoding='utf-8').read()
    assert 'v' + version + '"' in editor_text, 'editor version differs from VST factory/moduleinfo'
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
    synthdef_version, graphs = read_synthdef_manifest(generated_bytes)
    assert synthdef_version == 1 and len(graphs) == 28
    core_header = open(os.path.join(plugin_dir, 'foxdot_synthdefs.h'), encoding='utf-8').read()
    core_bytes = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', core_header))
    core_version, core_graphs = read_synthdef_manifest(core_bytes)
    assert core_version == 2 and len(core_graphs) == 26
    sample_header = open(os.path.join(plugin_dir, 'sample_synthdef.h'), encoding='utf-8').read()
    sample_bytes = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', sample_header))
    sample_version, sample_graphs = read_synthdef_manifest(sample_bytes)
    assert sample_version == 2 and set(sample_graphs) == {'DexterSample'}
    assert any(ugen[0] == 'PlayBuf' for ugen in sample_graphs['DexterSample'])
    assert not set(graphs).intersection(core_graphs)
    for graph, ugens in core_graphs.items():
        assert any(name == 'EnvGen' for name, _, _ in ugens), graph
        assert not any(name in ('MdaPiano', 'Decimator', 'PlayBuf') for name, _, _ in ugens), graph
    graphs.update(core_graphs)
    graphs.update(sample_graphs)
    graph_names_match = re.search(
        r'static constexpr auto kSynthGraphNames = std::array\{(.*?)\};',
        processor_source, re.S)
    assert graph_names_match, 'could not find the embedded synth graph registry'
    registered_graphs = re.findall(r'"(Dexter[^"]+)"', graph_names_match.group(1))
    assert len(registered_graphs) == len(graphs)
    assert set(registered_graphs) == set(graphs), \
        'the C++ graph registry must match the embedded SynthDef bank'
    synth_names_match = re.search(
        r'synthIndex\s*=\s*\((.*?)\)\.index\(pattern\.synth\)', api, re.S)
    assert synth_names_match, 'could not find the Python synth selector'
    synth_names = ast.literal_eval('(' + synth_names_match.group(1) + ')')
    assert len(synth_names) == len(registered_graphs) + 3, \
        'four base waveforms share DexterTone; every other player synth needs one graph'
    assert 'kVoiceSlotsPerPlayer = kMaxPolyphony * 2 + 1' in processor_source
    assert 'kPlayerCount * kVoiceSlotsPerPlayer' in processor_source
    assert 'kAudioVoiceCount * kMaxCachedSynthGraphsPerVoice' in processor_source
    assert 'kPlayerCount * kReverbGraphCount + 1' in processor_source
    assert 'kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity' in processor_source
    assert 'static_assert(kWorldNodeCapacity >= 2048' in processor_source
    assert 'scPoolCapacity(kSynthGraphCount + kReverbGraphCount)' in processor_source
    assert 'scPoolCapacity(kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity)' in processor_source
    assert 'prepareSynthGraphs(updates, updateMask, graphError)' in processor_source
    assert 'Graph_New(world_, synthDefinitions_[graphIndex]' in processor_source
    assert 'bool startImmediately = false' in processor_source
    assert 'if (!startImmediately)\n            Node_SetRun(&created->mNode, 0)' in processor_source
    assert 'graphIndex, graphError, true)' in processor_source
    assert 'worldAccessMutex_, std::try_to_lock' in processor_source
    init_block = re.search(
        r'bool initializeWorld\([^)]*\)\s*\{(.*?)\n    void discardPartialWorld',
        processor_source, re.S)
    assert init_block, 'could not find the embedded engine initialization routine'
    assert 'Graph_New(world_, synthDefinitions_' not in init_block.group(1), \
        'synth voice graphs must not all be constructed during plugin startup'
    assert 'static_cast<std::ptrdiff_t>(kVoiceSlotsPerPlayer)' in processor_source, \
        'voice allocation must stay within the owning player partition'
    assert 'kSynthGraphCount + kReverbGraphCount' in processor_source
    assert 'kSynthGraphCount + 3> mapping{}' in processor_source
    assert 'index <= 3 ? 0 : index - 3' in processor_source
    assert 'kSynthIndexToGraphIndex.back() == kSynthGraphCount - 1' in processor_source
    assert 'kSynthIndexToGraphIndex.size()' in processor_source
    graph_selector_match = re.search(
        r'static Graph\* graphForSynthIndex\(AudioVoice& voice, int synthIndex\)\s*\{(.*?)\n    \}',
        processor_source, re.S)
    assert graph_selector_match, 'could not find the synth-to-graph selector'
    assert 'synthIndex <= 24' not in graph_selector_match.group(1), \
        'the graph selector must not omit the last six embedded synths'
    synth_graph_mapping = [
        0 if index <= 3 else index - 3
        for index in range(len(registered_graphs) + 3)
    ]
    assert len(synth_graph_mapping) == len(synth_names)
    assert synth_graph_mapping[-6:] == list(range(len(registered_graphs) - 6,
                                                   len(registered_graphs)))
    assert ('LFSaw', 2, 2) in graphs['DexterDirt']
    assert ('LFNoise0', 1, 2) in graphs['DexterNoise']
    assert ('Vibrato', 9, 1) in graphs['DexterSoprano']
    assert ('Crackle', 1, 1) in graphs['DexterCrunch']
    assert ('Pulse', 2, 2) in graphs['DexterLazer']
    assert ('Gendy1', 10, 2) in graphs['DexterRave']
    assert ('Klank', 49, 2) in graphs['DexterBell']
    assert ('Klank', 49, 2) in graphs['DexterGong']
    assert ('Klank', 16, 2) in graphs['DexterMarimba']
    assert ('Trig1', 2, 2) in graphs['DexterBell']
    assert ('PinkNoise', 0, 2) in graphs['DexterMarimba']
    assert ('Decay', 2, 2) in graphs['DexterPluck']
    assert ('CombL', 4, 2) in graphs['DexterTwang']
    assert ('Ringz', 3, 2) in graphs['DexterDonk']
    assert ('LFNoise2', 1, 2) in graphs['DexterSpark']
    assert ('LFCub', 2, 2) in graphs['DexterBlip']
    assert ('Blip', 2, 2) in graphs['DexterBlip']
    assert ('Line', 4, 2) in graphs['DexterCharm']
    assert ('LFNoise0', 1, 2) in graphs['DexterScatter']
    assert ('LFPulse', 5, 2) in graphs['DexterOrient']
    assert ('Saw', 1, 2) in graphs['DexterZap']
    assert ('LFSaw', 4, 1) in graphs['DexterFuzz']
    assert ('Pulse', 2, 2) in graphs['DexterBug']
    assert ('SinOsc', 4, 2) in graphs['DexterRipple']
    for ugen_name in (b'\x06SinOsc', b'\x05LFTri', b'\x03Saw', b'\x05Pulse',
                      b'\x06MouseX', b'\x06MouseY', b'\x03LPF', b'\x03Lag',
                      b'DexterBass', b'DexterVarsaw', b'DexterDirt',
                      b'DexterJBass', b'DexterNoise', b'VarSaw',
                      b'DexterDub', b'DexterGrowl', b'DexterSoprano',
                      b'DexterCrunch', b'DexterLazer', b'DexterRave',
                      b'DexterBell', b'DexterGong', b'DexterMarimba',
                      b'DexterPluck', b'DexterTwang', b'DexterDonk',
                      b'DexterSquish', b'Klank', b'Trig1', b'PinkNoise',
                      b'Decay', b'CombL', b'Ringz',
                      b'LFSaw', b'LFNoise0', b'LFNoise2', b'LFCub', b'Blip',
                      b'Line', b'Vibrato', b'Crackle', b'Gendy1',
                      b'DexterCharm', b'DexterSpark', b'DexterBlip',
                      b'LFPulse', b'DexterScatter', b'DexterOrient',
                      b'DexterZap', b'DexterFuzz', b'DexterBug',
                      b'DexterRipple'):
        assert ugen_name in generated_bytes, 'missing SuperCollider oscillator/send UGen ' + repr(ugen_name)
    generator_text = open(generator, encoding='utf-8').read()
    assert 'ugen("LFSaw", 2, [(1, 0), (-1, 0)], [2])' in generator_text
    assert 'ugen("BinaryOpUGen", 2, [(11, 0), (10, 0)], [2], 0)' in generator_text
    assert 'ugen("BinaryOpUGen", 2, [(12, 0), (9, 0)], [2], 0)' in generator_text
    assert 'ugen("LFNoise0", 2, [(1, 0)], [2])' in generator_text
    assert 'ugen("Gendy1", 2,' in generator_text
    reverb_generator = os.path.join(repo_root, 'spikes', 'direct-world', 'testdata', 'generate_reverb.py')
    reverb_header = os.path.join(plugin_dir, 'reverb_synthdef.h')
    with tempfile.TemporaryDirectory() as directory:
        generated_reverbs = os.path.join(directory, 'reverb.scsyndef')
        subprocess.run([sys.executable, reverb_generator, generated_reverbs], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        generated_bytes = open(generated_reverbs, 'rb').read()
        header_text = open(reverb_header, encoding='utf-8').read()
        embedded_bytes = bytes(int(value, 16) for value in
                               re.findall(r'0x([0-9a-fA-F]{2})', header_text))
        assert generated_bytes == embedded_bytes, 'embedded reverb SynthDefs differ from their generator'
        for ugen_name in (b'FreeVerb', b'FreeVerb2', b'GVerb'):
            assert ugen_name in generated_bytes, 'missing SuperCollider reverb UGen ' + repr(ugen_name)

test_embedded_synthdef()

legacy = run_script('tempo(120)\nplay([60, 64], step=0.5, duration=0.4)\n')
assert len(legacy) == 1 and len(legacy[0]) == 22
fox_literal_rows = run_script('p1 >> saw(P[0, 2, None, (3, 5), 7], dur=0.5, oct=5, sus=0.4)\n')
assert len(fox_literal_rows) == 1 and fox_literal_rows[0][5] == '5', fox_literal_rows
assert legacy[0][5] == '2' and legacy[0][6] == '0'
assert legacy[0][7] == '1.0' and legacy[0][8] == '0.0'
assert legacy[0][10] == '0.0' and legacy[0][11] == '0.0'
assert legacy[0][12] == '0' and legacy[0][13] == '0'
assert legacy[0][14] == '0.01' and legacy[0][15] == '0.04'
assert legacy[0][16] == '-' and legacy[0][17] == '-' and legacy[0][18] == '-'
assert legacy[0][19] == '-' and legacy[0][20] == '-'
example7 = run_example('07-independent-players.py')
assert len(example7) == 2 and all(len(row) == 22 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.0'
example9 = run_example('09-foxdot-core.py')
assert len(example9) == 2 and all(len(row) == 22 for row in example9)
foxSaw, foxPulse = example9
assert foxSaw[1] == '0' and foxSaw[3] == '96.0' and foxSaw[6] == '2'
assert abs(float(foxSaw[7]) - 0.7) < 1e-6 and abs(float(foxSaw[8]) + 0.35) < 1e-6
assert foxPulse[1] == '1' and foxPulse[3] == '96.0' and foxPulse[6] == '3'
assert abs(float(foxPulse[7]) - 0.25) < 1e-6 and abs(float(foxPulse[8]) - 0.35) < 1e-6
example10Rows = run_example('10-pattern-transformations.py')
assert len(example10Rows) == 2 and all(len(row) == 22 for row in example10Rows)
assert example10Rows[0][5] == '7' and example10Rows[0][6] == '2'
assert example10Rows[1][5] == '4' and example10Rows[1][6] == '3'
example11Rows = run_example('11-foxdot-pattern-literals.py')
assert len(example11Rows) == 2 and all(len(row) == 22 for row in example11Rows)
assert example11Rows[0][5] == '7' and example11Rows[1][5] == '2'
example12Rows = run_example('12-pattern-arithmetic.py')
assert len(example12Rows) == 2 and all(len(row) == 22 for row in example12Rows)
assert example12Rows[0][5] == '5' and example12Rows[1][5] == '5'
example13Rows = run_example('13-pattern-methods.py')
assert len(example13Rows) == 5 and all(len(row) == 22 for row in example13Rows)
assert [row[5] for row in example13Rows] == ['8', '5', '4', '4', '4']
example14Rows = run_example('14-pattern-composition.py')
assert len(example14Rows) == 2 and all(len(row) == 22 for row in example14Rows)
assert [row[5] for row in example14Rows] == ['5', '7']
example15Rows = run_example('15-pattern-arithmetic-operators.py')
assert len(example15Rows) == 4 and all(len(row) == 22 for row in example15Rows)
assert [row[5] for row in example15Rows] == ['4', '2', '4', '4']
example16Rows = run_example('16-bounded-pattern-slices.py')
assert len(example16Rows) == 3 and all(len(row) == 22 for row in example16Rows)
assert [row[5] for row in example16Rows] == ['8', '4', '2']
example17Rows = run_example('17-live-player-edits.py')
assert len(example17Rows) == 2 and [row[1] for row in example17Rows] == ['0', '1']
example18Rows = run_example('18-ui-mouse-controls.py')
assert len(example18Rows) == 2 and all(len(row) == 22 for row in example18Rows)
assert example18Rows[0][12] == '1' and example18Rows[0][13] == '0'
assert example18Rows[1][12] == '0' and example18Rows[1][13] == '1'
envelopeRows = run_example('19-envelope-controls.py')
assert len(envelopeRows) == 2 and all(len(row) == 22 for row in envelopeRows)
assert float(envelopeRows[0][14]) == 0.005 and float(envelopeRows[0][15]) == 0.2
assert float(envelopeRows[1][14]) == 0.04 and float(envelopeRows[1][15]) == 0.1
timevarRows = run_example('20-timevar-controls.py')
assert len(timevarRows) == 2 and all(len(row) == 22 for row in timevarRows)
assert timevarRows[0][16] == 'v:0.15,0.45:4.0,4.0'
assert timevarRows[1][17] == 'l:-0.8,0.8:8.0,8.0'
assert timevarRows[1][18] == 'l:400.0,5000.0:8.0,8.0'
timevarNoteRows, timevarNoteEvents = run_script('Scale.default.set("minor")\np1 >> triangle(P[0, 1, 2, 3, 4, 5, 6, 0, 0, 1, 2, 3, 4, 5, 6, 0], dur=var([0.25, 0.5], [2, 2]), oct=var([4, 5], [2, 2]), sus=var([0.2, 0.4], [2, 2]), velocity=var([60, 110], [2, 2]))\n', include_events=True)
assert len(timevarNoteRows) == 1 and len(timevarNoteEvents) == 16
assert float(timevarNoteEvents[0][1]) == 0.0 and abs(float(timevarNoteEvents[0][2]) - 0.2) < 1e-9
assert int(timevarNoteEvents[0][3]) == 48 and int(timevarNoteEvents[0][4]) == 60
assert abs(float(timevarNoteEvents[0][5]) - 0.01) < 1e-9 and abs(float(timevarNoteEvents[0][6]) - 0.04) < 1e-9
assert abs(float(timevarNoteEvents[8][1]) - 2.0) < 1e-9
assert abs(float(timevarNoteEvents[8][2]) - 0.4) < 1e-9
assert int(timevarNoteEvents[8][3]) == 60 and int(timevarNoteEvents[8][4]) == 110
linNoteRows, linNoteEvents = run_script('Scale.default.set("minor")\np1 >> saw([0, 2, 4, 6, 0, 2, 4, 6], dur=linvar([0.25, 0.5], [1, 1]), oct=linvar([4, 5], [1, 1]), sus=linvar([0.1, 0.3], [1, 1]), velocity=linvar([55, 115], [1, 1]))\n', include_events=True)
assert len(linNoteRows) == 1 and len(linNoteEvents) == 8
assert abs(float(linNoteEvents[0][2]) - 0.1) < 1e-9 and int(linNoteEvents[0][3]) == 48
assert abs(float(linNoteEvents[1][1]) - 0.25) < 1e-9
assert abs(float(linNoteEvents[2][1]) - 0.5625) < 1e-9
assert abs(float(linNoteEvents[2][2]) - 0.2125) < 1e-9
assert int(linNoteEvents[2][3]) == 67 and int(linNoteEvents[2][4]) == 89
effectRows, effectEvents = run_script('p1 >> saw([0, 2, 4, 6], dur=0.5, echo=var([0.1, 0.5], [1, 1]), delay=linvar([0.0, 0.5], [1, 1]), delaytime=linvar([0.1, 0.5], [1, 1]), echomix=var([0.1, 0.5], [1, 1]), feedback=var([0.25, 0.45], [1, 1]), atk=var([0.01, 0.03], [1, 1]), rel=linvar([0.02, 0.1], [1, 1]))\n', include_events=True)
patternDelayRows, patternDelayEvents = run_script(
    'p1 >> saw([0, 2, 4, 5, 7], dur=0.5, delay=PDelay(3, 8))\n',
    include_events=True)
assert len(patternDelayRows) == 1 and len(patternDelayEvents) == 15
assert [round(float(event[7]), 6) for event in patternDelayEvents] == [
    0.0, 0.75, 1.5] * 5
chordDelayRows, chordDelayEvents = run_script(
    'p1 >> saw(P[(0, 2), (2, 4)], dur=1, sus=0.4, strum=0.05, delay=PDelay(2, 4))\n',
    include_events=True)
assert len(chordDelayRows) == 1 and [
    round(float(event[7]), 6) for event in chordDelayEvents
] == [0.0, 0.5, 0.05, 0.55, 0.0, 0.5, 0.05, 0.55]
assert [round(float(event[1]), 6) for event in chordDelayEvents] == [
    0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0]
tooManyDelayedEvents = run_error_script(
    'p1 >> saw([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] * 7 + [0, 1], '
    'dur=1, sus=0.1, delay=PDelay(3, 8))\n')
assert 'PDelay is limited to 256 expanded note events per player' in tooManyDelayedEvents
foxdotEchoRows, foxdotDecayRows = run_script('p1 >> saw([0, 2, 4, 6], dur=0.5, echo=0.5, decay=0.75, echomix=0.4, feedback=0.5)\n', include_decay=True)
assert foxdotDecayRows[0][0] == 'E' and float(foxdotDecayRows[0][2]) == 0.75 and foxdotDecayRows[0][3] == '-'
assert abs(float(foxdotEchoRows[0][10]) - 0.5) < 1e-9
assert abs(float(foxdotDecayRows[0][4]) - 0.4) < 1e-9 and float(foxdotDecayRows[0][6]) == 0.5 and foxdotDecayRows[0][8] == '-'
_, varDecayRows = run_script('p1 >> saw([0, 2], echo=0.5, decay=var([0.5, 1.5], [1, 1]), delay=linvar([0.1, 0.4], [1, 1]))\n', include_decay=True)
assert varDecayRows[0][3] == 'v:0.5,1.5:1.0,1.0' and len(varDecayRows[0]) == 9
assert varDecayRows[0][8] == 'l:0.1,0.4:1.0,1.0'
assert len(effectRows) == 1 and len(effectRows[0]) == 22 and len(effectEvents) == 4
assert effectRows[0][19] == 'v:0.1,0.5:1.0,1.0'
assert effectRows[0][20] == 'l:0.1,0.5:1.0,1.0'
assert abs(float(effectEvents[0][7]) - 0.0) < 1e-9
assert abs(float(effectEvents[1][7])) < 1e-9
assert abs(float(effectEvents[2][7])) < 1e-9
assert abs(float(effectEvents[0][5]) - 0.01) < 1e-9 and abs(float(effectEvents[0][6]) - 0.02) < 1e-9
assert abs(float(effectEvents[1][5]) - 0.01) < 1e-9 and abs(float(effectEvents[1][6]) - 0.06) < 1e-9
assert abs(float(effectEvents[2][5]) - 0.03) < 1e-9 and abs(float(effectEvents[2][6]) - 0.1) < 1e-9
effectExample = run_example('22-timevar-effects-envelopes.py')
assert len(effectExample) == 2 and all(len(row) == 22 for row in effectExample)
reverbRows, reverbEffects = run_script(open(os.path.join(sys.argv[2], '23-reverb-algorithms.py'), encoding='utf-8').read(), include_effects=True)
assert len(reverbRows) == 3 and len(reverbEffects) == 3
assert [int(row[2]) for row in reverbEffects] == [1, 2, 3]
assert [float(row[4]) for row in reverbEffects] == [0.65, 0.75, 0.8]
assert [float(row[7]) for row in reverbEffects] == [15.0, 15.0, 25.0]
assert all(abs(float(row[3]) - expected) < 1e-6 for row, expected in zip(reverbEffects, [0.2, 0.3, 0.25]))
filterRows, filterControls = run_script('p1 >> saw([0, 2, 4, 6], lpf=linvar([300, 3000], [2, 2]), lpr=var([0.35, 0.7], [2, 2]), hpf=var([0, 150], [2, 2]), hpr=0.8, bpf=linvar([500, 1200], [2, 2]), bpr=0.5)\n', include_filters=True)
assert len(filterRows) == 1 and len(filterRows[0]) == 22
assert len(filterControls) == 1 and len(filterControls[0]) == 14
assert filterControls[0][0:2] == ['F', '0']
assert filterControls[0][8] == 'l:300.0,3000.0:2.0,2.0'
assert filterControls[0][9] == 'v:0.35,0.7:2.0,2.0'
assert filterControls[0][12] == 'l:500.0,1200.0:2.0,2.0'
filterExampleRows, filterExampleControls = run_script(open(os.path.join(sys.argv[2], '24-foxdot-filters.py'), encoding='utf-8').read(), include_filters=True)
legacyEchoExample = run_example('25-echo-decay-compat.py')
assert len(legacyEchoExample) == 1 and legacyEchoExample[0][1] == '0'
scaleChordRows, scaleChordEvents = run_script(open(os.path.join(sys.argv[2], '26-scales-and-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(scaleChordRows) == 3 and [row[1] for row in scaleChordRows] == ['0', '1', '2']
assert [int(event[3]) for event in scaleChordEvents[:8]] == [60, 63, 67, 70, 67, 63, 62, 60]
assert [int(event[3]) for event in scaleChordEvents[8:14]] == [48, 51, 55, 50, 53, 57]
assert [int(event[3]) for event in scaleChordEvents[14:21]] == [60, 62, 63, 65, 67, 68, 70]
assert 'No unsupported constructs detected' in run_compatibility_check('Scale.default.set(Scale.melodicMinor)\np1 >> saw(P[(0, 2, 4), None], scale=[0, 2, 3, 5, 7, 8, 10])\n')
assert 'not available in the embedded scale set' in run_compatibility_check('Scale.notARealScale\n')
rootRows, rootEvents = run_script(open(os.path.join(sys.argv[2], '27-root-and-scale-controls.py'), encoding='utf-8').read(), include_events=True)
assert len(rootRows) == 3 and [row[1] for row in rootRows] == ['0', '1', '2']
assert [int(event[3]) for event in rootEvents] == [62, 65, 69, 72, 55, 58, 62, 57, 60, 63, 63, 67, 70, 74]
assert 'No unsupported constructs detected' in run_compatibility_check('Scale.default = "minor"\nRoot.default.set("Eb")\np1 >> saw([0, 2, 4], root=7)\n')
assert 'Root.default only supports .set(note)' in run_compatibility_check('Root.default.notes\n')
chordRows, chordEvents = run_script(open(os.path.join(sys.argv[2], '28-roman-numeral-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(chordRows) == 2 and [row[1] for row in chordRows] == ['0', '1']
assert [int(event[3]) for event in chordEvents[:14]] == [48, 52, 55, 53, 57, 48, 55, 47, 50, 65, 48, 52, 55, 59]
assert [int(event[3]) for event in chordEvents[14:]] == [69, 72, 76, 81]
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw(P[I, IV, V7, I7])\np2 >> saw(PGroup(0, 2, 4))\n')
strumRows, strumEvents = run_script(open(os.path.join(sys.argv[2], '29-strummed-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(strumRows) == 2 and [row[1] for row in strumRows] == ['0', '1']
assert [round(float(event[7]), 6) for event in strumEvents[:14]] == [0.0, 0.04, 0.08, 0.0, 0.04, 0.08, 0.12, 0.0, 0.04, 0.08, 0.0, 0.04, 0.08, 0.12]
assert [round(float(event[7]), 6) for event in strumEvents[14:]] == [0.0, 0.02, 0.04, 0.06, 0.0, 0.05, 0.1, 0.15, 0.0, 0.02, 0.04, 0.06, 0.0, 0.05, 0.1, 0.15]
assert all(abs(float(event[2]) - 1.75) < 1e-9 for event in strumEvents)
assert_player_polyphony_ok(strumRows[0], strumEvents[:14])
assert_player_polyphony_ok(strumRows[1], strumEvents[14:])
overlapStrumRows, overlapStrumEvents = run_script(
    'Clock.bpm = 70\nScale.default = Scale.major\nRoot.default.set("C")\n'
    'p1 >> triangle(P[I, V7, IV, I7], dur=2, sus=1.8, oct=2, strum=0.5, amp=0.32)\n'
    'p2 >> saw(P[V7, I7, IV7, V7], dur=2, sus=1.8, oct=3, '
    'strum=var([0.02, 0.15], 2), amp=0.18, pan=0.25)\n',
    include_events=True)
assert len(overlapStrumRows) == 2 and len(overlapStrumEvents) == 30
assert_player_polyphony_ok(overlapStrumRows[0], overlapStrumEvents[:14])
assert_player_polyphony_ok(overlapStrumRows[1], overlapStrumEvents[14:])
parse_start = source.find('bool parseResult(')
parse_end = source.find('\n}', parse_start)
parse_source = source[parse_start:parse_end]
assert "header.back() == '\\r'" in parse_source, 'Windows CRLF error headers must reach the Python traceback branch'
assert 'update.synthRate' in parse_source and 'update.synthIndex > 57' in parse_source
assert 'update.bpnoise' in source and 'update.swell' in source
processor_source = open(os.path.join(os.path.dirname(sys.argv[1]), 'processor.cpp'), encoding='utf-8').read()
assert 'advanceNoise1' in processor_source and 'swellEnvelopeAt' in processor_source
assert 'kReverbSendReleaseSeconds' in processor_source and 'beginReverbTail' in processor_source
assert '(voice.midiNote < 0 && !voice.releasing)' in processor_source
assert 'struct ActiveMidiGate' in processor_source and 'activeMidiGates_' in processor_source
assert 'event.noteOn.noteId = noteId' in processor_source
assert 'event.noteOff.noteId = noteId' in processor_source
assert 'foxDotEnvelopeDurationSeconds' in processor_source and 'safeSynthFrequency' in processor_source
assert 'synthIndex == 5 || synthIndex == 22' in processor_source
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw(P[I, V7], strum=0.06)\np2 >> saw(P[I7, V7], strum=var([0.02, 0.05], 2))\np1.mute()\np1.unmute()\n')
muteRows, muteCommands = run_script('p1.mute()\np2.unmute()\n', include_mutes=True)
assert muteRows == [] and muteCommands == [['M', '0', '1'], ['M', '1', '0']]
namespace['_dexter_events'] = []
namespace['_dexter_length'] = 1.0
namespace['_dexter_slots'] = 0
namespace['play']([(60, 64), (64, 67)], step=1, duration=1, strum=0.2)
assert len(namespace['_dexter_events']) == 4, 'same-pitch sustained overlaps are valid voices'
namespace['_dexter_events'] = []
namespace['_dexter_length'] = 0.0
namespace['_dexter_slots'] = 0
try:
    namespace['play']([(60, 61, 62, 63, 64), (65, 66, 67, 68)],
                      step=1, duration=1.5)
except ValueError as error:
    assert 'pattern exceeds 8-note polyphony' in str(error)
else:
    raise AssertionError('the eight-voice safety limit must remain enforced')
assert len(filterExampleRows) == 3 and [row[1] for row in filterExampleRows] == ['0', '1', '2']
assert len(filterExampleControls) == 3 and all(len(row) == 14 for row in filterExampleControls)
assert [row[1] for row in filterExampleControls] == ['0', '1', '2']
assert [row[8] for row in filterExampleControls] == ['l:300.0,1800.0:4.0,4.0', '-', '-']
assert [row[10] for row in filterExampleControls] == ['-', 'v:150.0,900.0:4.0,4.0', '-']
assert [row[12] for row in filterExampleControls] == ['-', '-', 'l:350.0,1200.0:4.0,4.0']
example30Rows = run_example('30-player-mute-controls.py')
assert len(example30Rows) == 2 and [row[1] for row in example30Rows] == ['0', '1']
example30Muted = open(os.path.join(sys.argv[2], '30-player-mute-controls.py'), encoding='utf-8').read().replace('#p2.mute()', 'p2.mute()')
assert 'No unsupported constructs detected' in run_compatibility_check(example30Muted)
mutedExampleRows, mutedExampleCommands = run_script(example30Muted, include_mutes=True)
assert len(mutedExampleRows) == 2 and mutedExampleCommands == [['M', '1', '1']]
example8 = run_example('08-synths-and-controls.py')
assert len(example8) == 2 and all(len(row) == 22 for row in example8)
p1, p2 = example8
assert p1[1] == '0' and p1[6] == '1' and abs(float(p1[7]) - 0.7) < 1e-6
assert abs(float(p1[8]) + 0.4) < 1e-6 and p1[9] == '5000.0'
assert p1[10] == '0.0' and p1[11] == '0.0'
assert p2[1] == '1' and p2[6] == '2' and abs(float(p2[7]) - 0.5) < 1e-6
assert abs(float(p2[8]) - 0.4) < 1e-6 and p2[9] == '1800.0'
assert p2[10] == '0.25' and p2[11] == '0.24'
example31 = run_example('31-bass-varsaw-synths.py')
assert len(example31) == 2
assert [int(row[6]) for row in example31] == [4, 5]
assert [float(row[21]) for row in example31] == [5.0, 0.32]
assert [float(row[14]) for row in example31] == [0.05, 0.04]
assert [float(row[15]) for row in example31] == [0.22, 0.22]
example32 = run_example('32-dirt-jbass-noise.py')
assert len(example32) == 3
assert [int(row[6]) for row in example32] == [7, 6, 8]
example33 = run_example('33-dub-growl-soprano.py')
assert len(example33) == 3
assert [int(row[6]) for row in example33] == [9, 10, 11]
assert [float(row[21]) for row in example33] == [0.0, 0.0, 2.0]
example34 = run_example('34-crunch-lazer-rave.py')
assert len(example34) == 3
assert [int(row[6]) for row in example34] == [12, 13, 14]
assert [float(row[21]) for row in example34] == [0.0, 2.5, 2.0]
example35 = run_example('35-resonant-bells.py')
assert len(example35) == 3 and [int(row[6]) for row in example35] == [15, 17, 16]
example36 = run_example('36-plucked-resonators.py')
assert len(example36) == 4 and [int(row[6]) for row in example36] == [18, 19, 20, 21]
example37Rows, example37Modulations = run_script(
    open(os.path.join(sys.argv[2], '37-bandpass-motion-and-swell.py'), encoding='utf-8').read(),
    include_modulations=True)
assert len(example37Rows) == 3 and len(example37Modulations) == 3
assert [row[0] for row in example37Modulations] == ['G', 'G', 'G']
assert [round(float(row[2]), 2) for row in example37Modulations] == [0.18, 0.0, 0.22]
assert [round(float(row[3]), 2) for row in example37Modulations] == [0.0, 1.2, 0.2]
assert example37Modulations[2][4].startswith('v:') and example37Modulations[2][5].startswith('l:')
example38Rows = run_example('38-bass-varsaw-release-check.py')
assert len(example38Rows) == 2 and [int(row[6]) for row in example38Rows] == [4, 5]
assert [float(row[14]) for row in example38Rows] == [0.04, 0.04]
assert [float(row[15]) for row in example38Rows] == [0.28, 0.28]
example39Rows, example39Events = run_script(
    open(os.path.join(sys.argv[2], '39-foxdot-native-bass-varsaw.py'), encoding='utf-8').read(),
    include_events=True)
assert [int(row[6]) for row in example39Rows] == [4, 5]
assert all(float(event[5]) == 0.01 and float(event[6]) == 0.04
           for event in example39Events)
example40Rows = run_example('40-charm-and-spark.py')
assert [int(row[6]) for row in example40Rows] == [22, 23]
example41Rows = run_example('41-blip-oscillator.py')
assert [int(row[6]) for row in example41Rows] == [24, 22]
example42Rows = run_example('42-self-contained-ugens.py')
assert [int(row[6]) for row in example42Rows] == [25, 26, 27, 28, 29, 30]
assert namespace['bug']([0]).synthRate == 1.0
core_synth_names = namespace['_NEW_SYNTHS']
assert len(core_synth_names) == 26
for number, name in enumerate(core_synth_names, 43):
    filename = f'{number:02d}-foxdot-{name}.py'
    rows = run_example(filename)
    active_rows = [row for row in rows if row[2] == '0']
    stopped_players = {int(row[1]) for row in rows if row[2] == '1'}
    assert len(active_rows) == 1 and int(active_rows[0][6]) == number - 12, (filename, rows)
    assert stopped_players == set(range(1, 8)), (filename, stopped_players)
    # Every name must actually evaluate and reach the wire protocol, not merely
    # be whitelisted by the static checker.
    assert namespace[name](0).synth == name
assert namespace['viola'](0).synthControls[0] == 6.0
assert namespace['prophet'](0).synthControls[4] == 0.4
assert namespace['scratch'](0).synthRate == 0.04
assert namespace['swell'](0).synthRate == 1.0
assert namespace['razz'](0).synthRate == 0.3
assert namespace['pads'](0).attackSeconds == 0.1
assert namespace['viola'](0, vib=8, fmod=0.5, blur=2).synthControls == (8, 0.5, 1, 0.5, 0.5, 0.5, 2)
for keyword, value in [('vib', float('inf')), ('rq', 0), ('blur', 0), ('lfowidth', 2)]:
    try:
        namespace['viola'](0, **{keyword: value})
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid synth control accepted: ' + keyword)
supported_report = run_compatibility_check('Clock.bpm = 120\nScale.default.set("minor")\np1 >> saw([0, 2, 4], dur=0.5, oct=5, sus=0.4, amp=0.7, pan=-0.2, echo=0.5, decay=0.75)\np2 >> pulse([0, None, 3], dur=1, cutoff=3000)\np3 >> saw(P[0, None, 2], dur=0.5)\np4 >> triangle([0, 2], pan=MouseX(), cutoff=MouseY(), atk=0.02, rel=0.15)\np5 >> saw([0, 2], amp=var([0.2, 0.6], [4, 4]), pan=linvar([-0.8, 0.8], 8), cutoff=linvar([400, 5000], 8))\np1 >> saw(P[0:8:2], dur=0.5)\np6 >> saw([0, 2], lpf=var([400, 5000], 4), lpr=0.45, hpf=300, hpr=0.8, bpf=1200, bpr=0.35, bpnoise=0.3, swell=1.0)\np1.stop()\n')
assert 'No unsupported constructs detected' in supported_report, supported_report
assert 'script was not executed' in supported_report, supported_report
unsupported_report = run_compatibility_check('from FoxDot import *\nClock.swing = 0.2\np9 >> bass([0, 2], dur=0.5)\nplay("x-o-")\nP["x-o"]\nP[0:]\nP[0:257]\nP[0:8:0]\nP[start:8]\nP[0] | ["x-o"]\n')
for phrase in ('Line 1:', 'Line 2:', 'Line 5:', 'P sample-symbol literals', 'finite stop', '256 generated items', 'step cannot be zero', 'integer-literal bounds', 'Pattern | and & do not accept sample strings'):
    assert phrase.lower() in unsupported_report.lower(), (phrase, unsupported_report)
syntax_report = run_compatibility_check('p1 >> saw([0])\nif :\n')
assert 'Python syntax error at column' in syntax_report and 'Line 2:' in syntax_report, syntax_report
for filename in os.listdir(sys.argv[2]):
    if filename.endswith('.py'):
        example_report = run_compatibility_check(open(os.path.join(sys.argv[2], filename), encoding='utf-8').read())
        assert 'No unsupported constructs detected' in example_report, (filename, example_report)
for scale_name in namespace['_SCALES']:
    namespace['Scale'].default.set(scale_name)
    assert namespace['Scale'].__dict__[scale_name] == scale_name
example69 = run_example('69-composed-star-and-karp.py')
assert [int(row[6]) for row in example69 if row[2] == '0'] == [55, 56, 0]
example70 = run_example('70-portable-sample-beat.py')
assert int(next(row for row in example70 if row[2] == '0')[6]) == 57
example71 = run_example('71-sample-selection-and-reverse.py')
assert [int(row[6]) for row in example71 if row[2] == '0'] == [57, 57]
example72 = run_example('72-var-sample-and-rate.py')
assert [int(row[6]) for row in example72 if row[2] == '0'] == [57, 57]
example73 = run_example('73-linvar-sample-and-rate.py')
assert [int(row[6]) for row in example73 if row[2] == '0'] == [57]
example74 = run_example('74-nested-play-strings.py')
assert [int(row[6]) for row in example74 if row[2] == '0'] == [57, 57]
example75 = run_example('75-random-and-layered-strings.py')
assert [int(row[6]) for row in example75 if row[2] == '0'] == [57, 57, 57]
example76 = run_example('76-sample-index-selection.py')
assert [int(row[6]) for row in example76 if row[2] == '0'] == [57]
example77 = run_example('77-pattern-sample-sequences.py')
assert len(example77) == 2
_, duplicateLayerRequests, duplicateLayerEvents = run_script(
    'p1 >> play("<x><x>", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in duplicateLayerRequests] == [120, 120]
assert len({int(row[3]) for row in duplicateLayerEvents}) == 2
players, requests = run_script('p1 >> play("x-o-", dur=0.5, sample=P[0, 1], rate=P[1, -1])', include_samples=True)
assert int(players[0][6]) == 57
assert [int(row[1]) for row in requests] == [120, 45, 111, 45]
assert [int(row[2]) for row in requests] == [0, 1, 0, 1]
assert [float(row[3]) for row in requests] == [1, -1, 1, -1]
assert all(row[4] == '0' for row in requests)
_, requests = run_script('p1 >> play("x ", sample=P[0, 1, 2])', include_samples=True)
assert [int(row[2]) for row in requests] == [0, 2, 1], requests
_, requests = run_script('p1 >> play("a", dur=0.5, sus=0.1, sample=P[-1, 0], rate=P[1, -1, 2, -2])', include_samples=True)
assert [float(row[3]) for row in requests] == [1, -1, 2, -2]
assert all(row[4] == '1' for row in requests)
_, sample_notes = run_script('p1 >> play("x", dur=P[0.5, 1])', include_events=True)
assert [float(row[1]) for row in sample_notes] == [0, 0.5]
varPlayers, varRequests, varControls = run_script(
    'p1 >> play("x-o-", sample=var([0, 1], [2, 2]), rate=var([1, -1], 4))',
    include_sample_controls=True)
assert len(varRequests) == 4
assert varControls == [['Q', '0', 'v:0.0,1.0:2.0,2.0', 'v:1.0,-1.0:4.0,4.0']], varControls
linPlayers, linRequests, linControls = run_script(
    'p1 >> play("x", sample=linvar([0, 1], 4), rate=linvar([0.75, 1.5], 4))',
    include_sample_controls=True)
assert linControls == [['Q', '0', 'l:0.0,1.0:4.0,4.0', 'l:0.75,1.5:4.0,4.0']], linControls
assert float(linRequests[0][3]) == 0.75
for bad in ('rate=0', 'rate=9', 'sample=0.5', 'sample=[]', 'rate=[]',
            'sample=var([0, 0.5], 4)', 'sample=linvar([0, 1.5], 4)',
            'rate=var([0.01, 1], 4)', 'rate=linvar([-1, 1], 4)'):
    failure = run_error_script('p1 >> play("x", ' + bad + ')')
    assert failure.startswith('ERROR\n') and 'ValueError' in failure, (bad, failure)
_, nestedRequests, nestedEvents = run_script(
    'p1 >> play("x[--]o", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in nestedRequests] == [120, 45, 45, 111]
assert [float(row[1]) for row in nestedEvents] == [0.0, 1.0, 1.5, 2.0]
_, lacedRequests, lacedEvents = run_script(
    'p1 >> play("(xo)---", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in lacedRequests] == [120, 45, 45, 45, 111, 45, 45, 45]
assert [float(row[1]) for row in lacedEvents] == [float(i) for i in range(8)]
_, layeredRequests, layeredEvents = run_script(
    'p1 >> play("<x  ><-  >", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in layeredRequests] == [120, 45]
assert [float(row[1]) for row in layeredEvents] == [0.0, 0.0]
_, randomRequests, randomEvents = run_script(
    'p1 >> play("{xo}", dur=1)', include_samples=True, include_events=True)
assert len(randomRequests) == 8 and len(randomEvents) == 8
assert len({int(row[1]) for row in randomRequests}) == 2
_, sampleIndexRequests = run_script(
    'p1 >> play("|x1|x|x0|x", sample=P[0, 1])', include_samples=True)
assert [int(row[1]) for row in sampleIndexRequests] == [120, 120, 120, 120]
assert [int(row[2]) for row in sampleIndexRequests] == [1, 1, 0, 1]
_, nestedSampleIndexRequests, nestedSampleIndexEvents = run_script(
    'p1 >> play("[|x1|x]o", dur=1, sample=0)',
    include_samples=True, include_events=True)
assert [int(row[2]) for row in nestedSampleIndexRequests] == [1, 0, 0]
assert [float(row[1]) for row in nestedSampleIndexEvents] == [0.0, 0.5, 1.0]
_, reverseSampleRequests = run_script(
    'p1 >> play(P["x", "o", " "].reverse(), sample=0)', include_samples=True)
assert [int(row[1]) for row in reverseSampleRequests] == [111, 120]
_, rotateSampleRequests = run_script(
    'p1 >> play(P["x", "o", " "].rotate(1), sample=0)', include_samples=True)
assert [int(row[1]) for row in rotateSampleRequests] == [111, 120]
_, repeatSampleRequests = run_script(
    'p1 >> play(P["x", "o"].repeat(2), sample=0)', include_samples=True)
assert [int(row[1]) for row in repeatSampleRequests] == [120, 111, 120, 111]
failure = run_error_script('p1 >> play(P["x", "o"] + P["a"])')
assert failure.startswith('ERROR\n') and 'TypeError' in failure
for badPattern in ('P["xx", "o"]', 'P[1, "x"]'):
    failure = run_error_script('p1 >> play(' + badPattern + ')')
    assert failure.startswith('ERROR\n') and 'TypeError' in failure, (badPattern, failure)
for badSelector in ('|x|', '|x10|', '|.1|', '|x1'):
    failure = run_error_script('p1 >> play("' + badSelector + '")')
    assert failure.startswith('ERROR\n') and 'ValueError' in failure, (badSelector, failure)
malformed_string = run_compatibility_check('p1 >> play("x[--")')
assert 'missing closing delimiter' in malformed_string
assert 'empty group' in run_compatibility_check('p1 >> play("x[]")')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("|x1|x|a1|a", sample=0)')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x", "o", " "].reverse(), sample=P[0, 1])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x", " "] | P["o", "-"], sample=P[0, 1])')
assert 'P sample-symbol & is unsupported' in run_compatibility_check(
    'p1 >> play(P["x"] & P["o"])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x"] | P["o"])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PAlt(P["x", " "], P["o", "-"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PAlt(P[0, 1], P[4, 5]))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PAlt(P[0, 1], P["x"]))')
assert 'at least two finite patterns' in run_compatibility_check(
    'p1 >> play(PAlt(P["x"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PStutter(PStretch(P["x", "o", " "], 6), P[1, 2]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PShuf(P["x", "o", " "]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PShuf(P[0, 1]))')
assert 'PShuf requires exactly one finite pattern' in run_compatibility_check(
    'p1 >> play(PShuf(P["x"], P["o"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PShuf(PJoin([P["x", " "], P["o", "-"]])))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PShuf(PJoin([P[0, 2], P[4, 7]])))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PJoin([P[0, 1], P["x"]]))')
assert 'PJoin requires one list or pattern' in run_compatibility_check(
    'p1 >> play(PJoin())')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x", dur=PBeat("x xx", start=1, dur=0.25))\n'
    'p2 >> play("o", dur=PDur(3, 8))')
assert 'PBeat expects a pulse string' in run_compatibility_check(
    'p1 >> play("x", dur=PBeat())')
assert 'PDur expects pulse and step counts' in run_compatibility_check(
    'p1 >> play("x", dur=PDur(3))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw(P[0, 2, 4], dur=PStrum(2), delay=PDelay(3, 8))')
assert 'PDelay expects pulse and step counts' in run_compatibility_check(
    'p1 >> saw([0], delay=PDelay(3))')
assert 'PStrum accepts one optional subdivision count' in run_compatibility_check(
    'p1 >> saw([0], dur=PStrum(2, 3))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], dur=PRhythm([1, (3, 8)]), sus=0.1)\n'
    'p2 >> sine([0, 2], delay=PQuicken(0.35, stepsize=2, steps=4))')
assert 'PQuicken accepts dur, stepsize, and steps' in run_compatibility_check(
    'p1 >> saw([0], delay=PQuicken(0.5, 2, 4, 5))')
assert 'PQuicken supports only dur, stepsize, and steps keywords' in run_compatibility_check(
    'p1 >> saw([0], delay=PQuicken(speed=2))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw(PZip(P[0, 2], P[4, 5]), dur=1, sus=0.5)\n'
    'p2 >> sine(PZip2(P[0, 1], P[2, 3], rule=lambda a, b: a != b))')
assert 'PZip requires 2 to 8 finite numeric patterns' in run_compatibility_check(
    'p1 >> saw(PZip(P[0]))')
assert 'PZip currently supports numeric note patterns and rests' in run_compatibility_check(
    'p1 >> play(PZip(P["x", "o"], P["-", " "] ))')
assert 'PRhythm expects one finite list or P pattern' in run_compatibility_check(
    'p1 >> saw([0], dur=PRhythm())')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PStretch(P[0, 1], 6))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PStutter(P[0, 1], P[1, 2]))')
assert 'PStretch requires a finite pattern and a target length' in run_compatibility_check(
    'p1 >> play(PStretch(P["x"]))')
assert 'PStutter requires a finite pattern and optional repeat counts' in run_compatibility_check(
    'p1 >> play(PStutter())')
assert 'P arithmetic over sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x", "o"] + P["a"])')
assert 'P sample-symbol literals' in run_compatibility_check('p1 >> play(P["xx", "o"])')
for badSelector in ('|x|', '|x10|', '|.1|', '|x1'):
    assert 'sample-index selector' in run_compatibility_check(
        'p1 >> play("' + badSelector + '")')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x[--]o(=[-o])")')
_, composedSampleRequests = run_script(
    'p1 >> play((P["x", " ", "x", " "] | P["o", "-", " ", "-"]).rotate(2), dur=0.5, sample=P[0, 1])',
    include_samples=True)
assert [int(row[1]) for row in composedSampleRequests] == [120, 111, 45, 45, 120]
assert [int(row[2]) for row in composedSampleRequests] == [0, 0, 1, 1, 0]
example78Rows, example78Requests = run_script(
    open(os.path.join(sys.argv[2], '78-sample-pattern-concatenation.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example78Rows) == 1 and len(example78Requests) == 5
example79Rows, example79Requests = run_script(
    open(os.path.join(sys.argv[2], '79-sample-pattern-alternation.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example79Rows) == 1 and [int(row[1]) for row in example79Requests] == [
    120, 111, 45, 120, 111, 120, 45]
example80Rows, example80Requests = run_script(
    open(os.path.join(sys.argv[2], '80-sample-pattern-stretch-stutter.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example80Rows) == 1 and [int(row[1]) for row in example80Requests] == [
    120, 111, 111, 120, 120, 111]
example81Rows, example81Requests = run_script(
    open(os.path.join(sys.argv[2], '81-sample-pattern-shuffle.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example81Rows) == 1 and sorted(int(row[1]) for row in example81Requests) == [
    45, 111, 111, 120, 120]
example82Rows, example82Requests = run_script(
    open(os.path.join(sys.argv[2], '82-sample-pattern-join-shuffle.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example82Rows) == 1 and sorted(int(row[1]) for row in example82Requests) == [
    45, 111, 120, 120]
example83Rows, example83Requests, example83Events = run_script(
    open(os.path.join(sys.argv[2], '83-pbeat-pdur-rhythm-durations.py'), encoding='utf-8').read(),
    include_samples=True, include_events=True)
assert len(example83Rows) == 2 and len(example83Requests) == 7 and len(example83Events) == 7
assert sorted(round(float(row[1]), 6) for row in example83Events) == [
    0.0, 0.0, 0.5, 0.75, 1.25, 1.5, 1.5]
example84Rows, example84Events = run_script(
    open(os.path.join(sys.argv[2], '84-pdelay-pstrum-combination.py'), encoding='utf-8').read(),
    include_events=True)
assert len(example84Rows) == 1 and len(example84Events) == 36
assert [round(float(event[7]), 6) for event in example84Events] == [
    0.0, 0.75, 1.5] * 12
assert [round(float(event[1]), 6) for event in example84Events] == [
    beat for beat in (0.0, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 5.0, 5.5, 6.0, 6.5, 7.0)
    for _ in range(3)]
rhythmGroupRows, rhythmGroupEvents = run_script(
    'p1 >> triangle(P[0, 2], dur=PRhythm([1, (3, 8)]), sus=0.1)\n',
    include_events=True)
assert len(rhythmGroupRows) == 1 and len(rhythmGroupEvents) == 4
assert [round(float(event[1]), 6) for event in rhythmGroupEvents] == [
    0.0, 1.0, 1.0, 1.0]
assert [round(float(event[7]), 6) for event in rhythmGroupEvents] == [
    0.0, 0.0, 0.75, 1.5]
quickenRows, quickenEvents = run_script(
    'p1 >> sine(P[0, 2], dur=1, sus=0.02, delay=PQuicken(0.4, 2, 3))\n',
    include_events=True)
assert len(quickenRows) == 1 and len(quickenEvents) == 6
assert [round(float(event[7]), 6) for event in quickenEvents] == [
    0.0, 0.2, 0.3, 0.0, 0.2, 0.3]
zipRows, zipEvents = run_script(
    'p1 >> sine(PZip(P[None, 2], P[3, 4]), dur=1, sus=0.5)\n',
    include_events=True)
assert len(zipRows) == 1 and len(zipEvents) == 3
example86Rows, example86Events = run_script(
    open(os.path.join(sys.argv[2], '86-pzip-pzip2-chords.py'), encoding='utf-8').read(),
    include_events=True)
assert len(example86Rows) == 2 and len(example86Events) == 28
overlapExampleText = open(
    os.path.join(sys.argv[2], '87-overlapping-sustain-voices.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(overlapExampleText)
overlapRows, overlapEvents = run_script(overlapExampleText, include_events=True)
assert len(overlapRows) == 2 and len(overlapEvents) == 10
assert [float(event[2]) for event in overlapEvents[:4]] == [1.0] * 4
assert [float(event[2]) for event in overlapEvents[4:]] == [1.5] * 6
assert_player_polyphony_ok(overlapRows[0], overlapEvents[:4])
assert_player_polyphony_ok(overlapRows[1], overlapEvents[4:])
example85Rows, example85Requests, example85Events = run_script(
    open(os.path.join(sys.argv[2], '85-pquicken-prhythm.py'), encoding='utf-8').read(),
    include_samples=True, include_events=True)
assert len(example85Rows) == 2 and len(example85Requests) == 16
assert len(example85Events) == 32
assert sorted(round(float(event[7]), 6) for event in example85Events
              if abs(float(event[2]) - 0.02) < 1e-9) == [
    0.0, 0.0, 0.0, 0.0, 0.175, 0.175, 0.175, 0.175,
    0.2625, 0.2625, 0.2625, 0.2625,
    0.30625, 0.30625, 0.30625, 0.30625]
numericTransformText = open(
    os.path.join(sys.argv[2], '88-numeric-pattern-transforms.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(numericTransformText)
numericTransformRows, numericTransformEvents = run_script(
    numericTransformText, include_events=True)
assert len(numericTransformRows) == 3 and len(numericTransformEvents) == 30
eventOffset = 0
for row, eventCount in zip(numericTransformRows, (12, 6, 12)):
    playerEvents = numericTransformEvents[eventOffset:eventOffset + eventCount]
    eventOffset += eventCount
    assert_player_polyphony_ok(row, playerEvents)
sample_directory = os.path.join(sys.argv[2], 'Samples')
assert len([name for path, dirs, files in os.walk(sample_directory) for name in files if name.endswith('.wav')]) == 6
print('Smoke tests passed: embedded runtime, pattern API, static FoxDot compatibility checks, line diagnostics, and Examples 01-88.')
