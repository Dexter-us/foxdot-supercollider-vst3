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
for airwindows_layout_fragment in (
    'const uint32 expectedControlCount = isTapeDelay ? 8u : 6u;',
    'userDefinition.outControl = 0;',
    'userDefinition.inputBusControl = 1;',
    'std::array<int, 7>{{2, 3, 4, 5, 6, -1, 7}}',
    'std::array<int, 7>{{2, 3, 4, -1, -1, -1, 5}}',
):
    if airwindows_layout_fragment not in processor_source:
        raise AssertionError(
            'Persistent Airwindows control ABI is missing or out of sync: ' +
            airwindows_layout_fragment
        )
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

def make_wrapper(source_path, result_path, user_synth_names=()):
    parts = []
    placeholders = {'namespaceName': 'test_scope', 'api': api,
                    'sourcePath': source_path, 'resultPath': result_path,
                    'userSynthNames': tuple(user_synth_names)}
    for token in re.findall(r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\)|pythonStringTuple\([^)]*\))', wrapper_cpp):
        if token.startswith('"'):
            parts.append(ast.literal_eval(token))
        elif token.startswith('pythonStringTuple('):
            parts.append(repr(placeholders['userSynthNames']))
        else:
            argument = token[len('pythonLiteral('):-1]
            if argument not in placeholders:
                raise SystemExit('Unknown generated-wrapper placeholder: ' + argument)
            parts.append(repr(placeholders[argument]))
    return ''.join(parts).replace(
        'int(time.monotonic_ns() & 0xffffffff)', '123456789')

safe_names = ('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
              'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
              'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str',
              'sum', 'tuple', 'zip', 'ValueError', 'TypeError',
              'ZeroDivisionError', '__build_class__', 'ord', 'chr')
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_bpm_control': None,
    '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_root_control': None,
    '_dexter_updates': {}, '_dexter_mutes': {},
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
randChoices = namespace['PRand'](namespace['P'][0, 2, 4, 7],
                                 length=64, seed=12345)
assert len(randChoices) == 64 and all(value in (0, 2, 4, 7) for value in randChoices)
assert randChoices == namespace['PRand'](
    namespace['P'][0, 2, 4, 7], length=64, seed=12345)
assert namespace['PRand'](5, length=64, seed=8) == namespace['PRand'](
    0, 5, length=64, seed=8)
integerRange = namespace['PRand'](-3, 4, length=64, seed=9)
assert all(isinstance(value, int) and -3 <= value <= 4 for value in integerRange)
randomSamples = namespace['PRand'](
    namespace['P']['x', 'o', '-', ' '], length=64, seed=99)
assert all(value in ('x', 'o', '-', ' ') for value in randomSamples)
white = namespace['PWhite'](-1.0, 1.0, length=256, seed=123)
assert len(white) == 256 and all(-1.0 <= value <= 1.0 for value in white)
assert white == namespace['PWhite'](-1.0, 1.0, length=256, seed=123)
assert white != namespace['PWhite'](-1.0, 1.0, length=256, seed=124)
assert abs(sum(white) / len(white)) < 0.15
assert namespace['PWhite'](0.5, 0.5, length=8, seed=2) == [0.5] * 8
walk = namespace['PWalk'](max=7, step=namespace['P'][1, 2, 1],
                          start=0, length=128, seed=123)
assert len(walk) == 128 and walk[0] == 0
assert walk == namespace['PWalk'](
    max=7, step=namespace['P'][1, 2, 1], start=0, length=128, seed=123)
assert all(-9 <= value <= 9 for value in walk)
boundedWalk = namespace['PWalk'](max=3, step=1, start=0, length=256, seed=9)
assert all(-3 <= value <= 3 for value in boundedWalk)
assert namespace['PWalk'](max=-2, step=-1, start=1, length=4, seed=3)[0] == 1
chainMap = {0: [1, 2], 1: [0], 2: [2, 9]}
chain = namespace['PChain'](chainMap, length=128, seed=123)
assert len(chain) == 128 and chain[0] == 0
assert chain == namespace['PChain'](chainMap, length=128, seed=123)
for previous, current in zip(chain, chain[1:]):
    if previous in chainMap:
        assert current in chainMap[previous]
    else:
        assert current == previous
terminalChain = namespace['PChain']({0: [9]}, length=8, seed=6)
assert terminalChain == [0, 9, 9, 9, 9, 9, 9, 9]
sampleChain = namespace['PChain'](
    {'x': ['o', '-'], 'o': ['x'], '-': ['x']},
    length=32, seed=22)
assert len(sampleChain) == 32 and sampleChain[0] == 'x'
assert all(value in ('x', 'o', '-') for value in sampleChain)
assert namespace['PChain']({5: [5]}, length=6) == [5] * 6
assert namespace['PDelta'](
    namespace['P'][1, -2, 0.5], start=60, length=7) == [
        60, 61, 59, 59.5, 60.5, 58.5, 59]
assert namespace['PDelta'](2, start=-1, length=4) == [-1, 1, 3, 5]
assert namespace['PDelta'](
    namespace['P'][1, 2], start=0, length=6) == [0, 1, 3, 4, 6, 7]
assert namespace['PSquare']()[:10] == [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
assert len(namespace['PSquare']()) == 256
assert namespace['PSquare'](length=1) == [0]
assert namespace['PSq']() == [1, 4, 9]
assert namespace['PSq'](-3, 2, 4) == [9, 4, 1, 0]
assert namespace['PRange'](5) == [0, 1, 2, 3, 4]
assert namespace['PRange'](1, 6, 2) == [1, 3, 5]
assert namespace['PRange'](5, 0) == [5, 4, 3, 2, 1]
assert namespace['PRange'](3, 3) == [3]
assert namespace['PStep'](4, 1) == [0, 0, 0, 1]
assert namespace['PStep'](4, 'x', '-') == ['-', '-', '-', 'x']
assert namespace['PEuclid'](3, 8) == [1, 0, 0, 1, 0, 0, 1, 0]
assert namespace['PEuclid2'](3, 8, ' ', 'x') == [
    'x', ' ', ' ', 'x', ' ', ' ', 'x', ' ']
assert namespace['P'][0, 2, 2, 4].undup() == [0, 2, 4]
assert namespace['P'][0, 2, 4].norm() == [0.0, 0.5, 1.0]
pvarPitched = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][4, 6]], [1, 1])
assert pvarPitched.expand(0.5) == [0, 2, 4, 6]
pvarVariableDur = namespace['Pvar'](
    [namespace['P'][0, 2, 4, 6], namespace['P'][7, 5, 3, 1]], [3, 3])
assert pvarVariableDur.expand(namespace['P'][0.5, 1.0]) == [
    0, 2, 4, 6, 7, 5, 3, 1]
pvarVarDur = namespace['Pvar'](
    [namespace['P'][0, 2, 4], namespace['P'][7, 5, 3]], [2, 2])
assert pvarVarDur.expand(namespace['var']([0.5, 1.0], [1, 1])) == [
    0, 2, 4, 7, 5, 3]
pvarLinvarDur = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][7, 5]], [1, 1])
assert pvarLinvarDur.expand(
    namespace['linvar']([0.25, 1.25], [0.5, 0.5])) == [0, 2, 7, 5]
pvarGroupedDur = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][4, 6]], [3, 3])
pvarGroupedRhythm = namespace['PRhythm']([1, (3, 8)])
assert pvarGroupedDur.expand(pvarGroupedRhythm) == [0, 2, 4, 6]
pvarGroupedSampleDur = namespace['Pvar'](['x-', 'o-'], [3, 3])
assert pvarGroupedSampleDur.expand(pvarGroupedRhythm) == list('x-o-')
pvarBroadcast = namespace['Pvar'](
    [namespace['P'][0, 1], namespace['P'][2]], 1)
assert pvarBroadcast.expand(0.5) == [0, 1, 2, 2]
assert namespace['Pvar'](
    patterns=[namespace['P'][0], namespace['P'][2]], dur=0.5
).expand(0.5) == [0, 2]
pvarSamples = namespace['Pvar'](
    ['x-o-', 'o--x'], [2, 2])
assert pvarSamples.kind == 'sample'
assert pvarSamples.expand(0.5) == list('x-o-o--x')
for pvarFactory in (
        lambda: namespace['Pvar']([], 1),
        lambda: namespace['Pvar']([namespace['P'][0]], []),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [1, 2, 3]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P']['x']], 1),
        lambda: namespace['Pvar']([namespace['P'][0]], float('inf')),
        lambda: namespace['Pvar']([namespace['P'][0]] * 2, [4096, 4096]),
        lambda: namespace['Pvar']([namespace['P'][0]] * 9, 1),
        lambda: namespace['Pvar']([[0] * 257], 1),
        lambda: pvarPitched.expand(0.3),
        lambda: pvarPitched.expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [1, 1]
        ).expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [3, 2]
        ).expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [2, 2]
        ).expand(namespace['var']([0.5, 1.0], [1.0, 1.5])),
        lambda: pvarGroupedDur.expand(namespace['P'][
            namespace['_FoxDotDurationGroup']([1.0, 9.0])]),
        lambda: pvarGroupedDur.expand(namespace['P'][
            namespace['_FoxDotDurationGroup']([0.5, 0.75])]),
        lambda: pvarVarDur.expand(
            namespace['var']([0.05, 1.0], [1.0, 1.0])),
        lambda: namespace['Pvar'](
            [namespace['P'][0]] * 8, 1).expand(0.001)):
    try:
        pvarFactory()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('Pvar accepted an invalid phrase, duration, or expansion')
for generator in (
        lambda: namespace['PRand']([], length=2),
        lambda: namespace['PRand'](2.5, length=2),
        lambda: namespace['PRand'](4, 2, length=2),
        lambda: namespace['PRand'](1, length=0),
        lambda: namespace['PRand'](1, length=257),
        lambda: namespace['PRand'](1, seed=True),
        lambda: namespace['PWhite'](2.0, 1.0),
        lambda: namespace['PWhite'](0.0, float('inf')),
        lambda: namespace['PWhite'](0.0, 1.0, length=257),
        lambda: namespace['PWalk'](max=True),
        lambda: namespace['PWalk'](
            step=namespace['_FoxDotFinitePattern']([]), length=3),
        lambda: namespace['PWalk'](step=namespace['P'][1, None], length=3),
        lambda: namespace['PWalk'](step=namespace['P']['x', 'o'], length=3),
        lambda: namespace['PWalk'](step=float('inf')),
        lambda: namespace['PWalk'](length=257),
        lambda: namespace['PWalk'](seed=True),
        lambda: namespace['PChain']([]),
        lambda: namespace['PChain']({}),
        lambda: namespace['PChain']({True: [0]}),
        lambda: namespace['PChain']({0: []}),
        lambda: namespace['PChain']({0: ['x']}),
        lambda: namespace['PChain']({0: [object()]}),
        lambda: namespace['PChain']({0: [0]}, length=257),
        lambda: namespace['PChain']({0: [0]}, seed=True),
        lambda: namespace['PChain']({i: [i] for i in range(33)}),
        lambda: namespace['PChain'](
            {0: list(range(257))}),
        lambda: namespace['PDelta'](
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['PDelta'](namespace['P']['x', 'o']),
        lambda: namespace['PDelta'](namespace['P'][1, None]),
        lambda: namespace['PDelta'](1, start=float('inf')),
        lambda: namespace['PDelta'](1, length=257),
        lambda: namespace['PDelta'](1000000, start=1, length=2),
        lambda: namespace['PSquare'](length=0),
        lambda: namespace['PSquare'](length=257),
        lambda: namespace['PSquare'](length=1.5),
        lambda: namespace['PSq'](c=0),
        lambda: namespace['PSq'](c=257),
        lambda: namespace['PSq'](b=float('inf')),
        lambda: namespace['PSq'](b=32),
        lambda: namespace['PSq'](-2, 0.5, 2),
        lambda: namespace['PRange'](0, 257),
        lambda: namespace['PRange'](0, 8, 0),
        lambda: namespace['PRange'](True, 4),
        lambda: namespace['PRange'](0, 1000000),
        lambda: namespace['PStep'](0, 1),
        lambda: namespace['PStep'](257, 1),
        lambda: namespace['PStep'](4, 'x'),
        lambda: namespace['PEuclid'](9, 8),
        lambda: namespace['PEuclid'](1, 257),
        lambda: namespace['PEuclid'](1, 8.0),
        lambda: namespace['PEuclid2'](2, 5, 0, 'x')):
    try:
        generator()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('bounded pattern generator should reject invalid bounds')
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
assert namespace['P'][0, 1, 2].stretch(5) == [0, 1, 2, 0, 1]
assert namespace['P']['x', 'o'].stretch(namespace['P'][2, 3]) == [
    'x', 'o', 'x', 'o', 'x']
assert namespace['P'][1, 2].loop(2) == [1, 2, 1, 2]
assert namespace['P'][1, 2].loop(namespace['P'][1, 2]) == [
    1, 2, 1, 2, 1, 2]
assert namespace['P'][1, 2].loop(0) == []
assert namespace['P'][1, 2].stretch(0) == []
shuffleCounterBeforeMethodTests = namespace['_dexter_shuffle_counter']
assert namespace['P'][0, 2, 4, 7].invert() == [7, 5, 3, 0]
assert namespace['P'][0, None, 4].invert() == [4, None, 0]
assert namespace['P'][-4, -2, 0].invert() == [4, 2, 0]
assert namespace['P'][0, 0].invert() == [0, 0]
invertedChords = namespace['P'][
    0, namespace['PGroup'](0, 4, 7), 7].invert()
assert invertedChords == [7, (7, 3, 0), 0]
assert isinstance(invertedChords[1], namespace['_FoxDotChord'])
assert namespace['_FoxDotFinitePattern']([]).invert() == []
assert namespace['P'][0, 1, 2, 3].arp(namespace['P'][0, 2]) == [
    0, 2, 1, 3, 2, 4, 3, 5]
arpeggiatedChords = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].arp(namespace['P'][0, 2])
assert arpeggiatedChords == [(0, 4, 7), (2, 6, 9), 2, 4]
assert namespace['P'][0, None].arp(namespace['P'][0, 2]) == [
    0, 2, None, None]
assert namespace['P'][0, 1, 2, 3].splice(
    namespace['P'][4, 5, 6, 7], namespace['P'][8, 9]) == [
        0, 4, 8, 1, 5, 9, 2, 6, 8, 3, 7, 9]
assert namespace['P']['x', '-', 'o'].splice('  ', 'x-') == [
    'x', ' ', 'x', '-', ' ', '-', 'o', ' ', 'x',
    'x', ' ', '-', '-', ' ', 'x', 'o', ' ', '-']
assert namespace['P'][0, namespace['PGroup'](0, 3, 7)].splice(
    namespace['P'][4, 5]) == [
        0, 4, (0, 3, 7), 5]
assert namespace['P'][3, 1, 2, 1].sort() == [1, 1, 2, 3]
assert namespace['P'][-2, 4, 0].sort() == [-2, 0, 4]
assert namespace['P']['x', 'o', '-', 'x'].sort() == ['-', 'o', 'x', 'x']
assert namespace['_FoxDotFinitePattern']([]).sort() == []
assert namespace['P'][1, 1, 2, 1, 1, 3, 3].unduplicate() == [
    1, 2, 1, 3]
assert namespace['P'][None, None, 1, 1, None, None].unduplicate() == [
    None, 1, None]
assert namespace['P']['x', 'x', '-', '-', 'o', 'x', 'x'].unduplicate() == [
    'x', '-', 'o', 'x']
assert namespace['P'][1, namespace['PGroup'](2, 4), namespace['PGroup'](2, 4),
                      3].unduplicate() == [1, (2, 4), 3]
assert namespace['P'][0, 2, 4, 2].replace(2, 7) == [0, 7, 4, 7]
assert namespace['P'][0, None, 4].replace(None, 2) == [0, 2, 4]
assert namespace['P']['x', '-', 'x'].replace('x', 'o') == ['o', '-', 'o']
replacedChord = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].replace(4, 5)
assert replacedChord == [(0, 5, 7), 2]
assert isinstance(replacedChord[0], namespace['_FoxDotChord'])
nestedPattern = namespace['_FoxDotFinitePattern']([
    namespace['P'][0, 2], namespace['PGroup'](2, 4)])
replacedNested = nestedPattern.replace(2, 3)
assert replacedNested == [[0, 3], (3, 4)]
assert isinstance(replacedNested[0], namespace['_FoxDotFinitePattern'])
assert isinstance(replacedNested[1], namespace['_FoxDotChord'])
replacementPattern = namespace['P'][8, 9]
replacementResult = namespace['P'][1, 2].replace(2, replacementPattern)
assert replacementResult == [1, [8, 9]]
assert replacementResult[1] is replacementPattern
untouchedPattern = namespace['P'][1, 2]
assert untouchedPattern.replace(3, 4) == [1, 2]
assert untouchedPattern == [1, 2]
assert namespace['_FoxDotFinitePattern']([]).replace(1, 2) == []
assert namespace['P'][0, 1, 2, 3].layer('reverse') == [
    (0, 3), (1, 2), (2, 1), (3, 0)]
assert namespace['P'][0, 1, 2].layer('rotate', 1) == [
    (0, 1), (1, 2), (2, 0)]
assert namespace['P'][0, namespace['PGroup'](0, 4, 7)].layer('reverse') == [
    (0, 0, 4, 7), (0, 4, 7, 0)]
assert namespace['P']['x', 'o', '-'].layer('reverse') == [
    ('x', '-'), ('o', 'o'), ('-', 'x')]
assert namespace['P'][0, 1].layer('replace', 1, 2) == [(0, 0), (1, 2)]
assert namespace['P'][0, 1, 2, 3].trim(2) == [0, 1]
assert namespace['P'][0, 1, 2, 3].trim(-1) == [0, 1, 2]
assert namespace['P'][0, 1, 2, 3].trim(0) == []
assert namespace['P'][0, 1, 2, 3].ltrim(2) == [2, 3]
assert namespace['P'][0, 1, 2, 3].ltrim(-1) == [1, 2, 3]
assert namespace['P'][0, 1, 2, 3].ltrim(0) == [0, 1, 2, 3]
assert namespace['P']['x', '-', 'o'].trim(2) == ['x', '-']
assert namespace['P'][namespace['PGroup'](0, 4, 7), 2].ltrim(1) == [2]
assert namespace['_FoxDotFinitePattern']([]).trim(2) == []
assert namespace['P'][0, 2, 4].normalise() == [0.0, 0.5, 1.0]
assert namespace['P'][-4, 0, 4].normalise() == [0.0, 0.5, 1.0]
assert namespace['P'][5, 5, 5].normalise() == [0.0, 0.0, 0.0]
normalizedPattern = namespace['P'][-10, -5, 0, 5].normalise()
assert all(0.0 <= value <= 1.0 for value in normalizedPattern)
for normalize_call in (
        lambda: namespace['_FoxDotFinitePattern']([]).normalise(),
        lambda: namespace['P'][0, None, 2].normalise(),
        lambda: namespace['P'][namespace['PGroup'](0, 2), 4].normalise(),
        lambda: namespace['P']['x', 'o'].normalise()):
    try:
        normalize_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('normalise should require a non-empty numeric pattern')
chooseCounterBefore = namespace['_dexter_random_counter']
namespace['_dexter_random_counter'] = 0
chosenOnce = namespace['P'][0, 2, 4, 7].choose()
namespace['_dexter_random_counter'] = 0
chosenAgain = namespace['P'][0, 2, 4, 7].choose()
assert chosenOnce == chosenAgain and chosenOnce in (0, 2, 4, 7)
chordChoice = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].choose()
assert chordChoice in (namespace['PGroup'](0, 4, 7), 2)
sampleChoice = namespace['P']['x', '-', 'o'].choose()
assert sampleChoice in ('x', '-', 'o')
namespace['_dexter_random_counter'] = chooseCounterBefore
for choose_call in (
        lambda: namespace['_FoxDotFinitePattern']([]).choose(),
        lambda: namespace['P'][0, 'x'].choose()):
    try:
        choose_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('choose should require a non-empty finite pattern')
assert namespace['P'][0, 2, 4].zip(namespace['P'][1, 3, 5]) == [
    (0, 1), (2, 3), (4, 5)]
assert namespace['P'][0, 2, 4].zip(namespace['P'][1, 3]) == [
    (0, 1), (2, 3), (4, 1), (0, 3), (2, 1), (4, 3)]
assert namespace['P'][0, 2].zip(namespace['P'][1, 3, 5]) == [
    (0, 1), (2, 3), (0, 5), (2, 1), (0, 3), (2, 5)]
assert namespace['P'][namespace['PGroup'](0, 4), 2].zip(
    namespace['P'][1, 3]) == [(0, 4, 1), (2, 3)]
assert namespace['P']['x', 'o'].zip(namespace['P']['h', 't']) == [
    ('x', 'h'), ('o', 't')]
deepZipped = namespace['P'][
    namespace['P'][1, 2], 3].deepzip(namespace['P'][4, 5])
assert deepZipped == [
    namespace['P'][(1, 4), (2, 4)], (3, 5)]
deepRight = namespace['P'][1, 2].deepzip(
    namespace['P'][namespace['P'][3, 4], 5])
assert deepRight == [
    namespace['P'][(1, 3), (1, 4)], (2, 5)]
deepReversed = namespace['P'][
    namespace['P'][1, 2], 3].deeprzip(namespace['P'][4, 5])
assert deepReversed == [
    namespace['P'][(4, 1), (4, 2)], (5, 3)]
assert namespace['P'][1, 2].zip(3) == [(1, 3), (2, 3)]
assert namespace['P'][1, 3, 4].map({1: 6, 4: 2}) == [6, 0, 2]
assert namespace['P'][
    namespace['P'][1, 3],
    namespace['PGroup'](0, 4)].map({1: 7, 3: 2, 0: 4, 4: 6}) == [
        [7, 2], namespace['PGroup'](4, 6)]
assert namespace['P']['x', 'o'].map({'x': 'o', 'o': 'x'}) == ['o', 'x']
assert namespace['P'][0, None].map({}) == [0, 0]
assert namespace['P'][1].map({1: (2, 4)}) == [(2, 4)]
assert namespace['P'][0, 1, 2, 3].pivot(0) == [0, 3, 2, 1]
assert namespace['P'][0, 1, 2, 3].pivot(1) == [2, 1, 0, 3]
assert namespace['P'][0, 1, 2, 3].pivot(3) == [2, 1, 0, 3]
assert namespace['P'][0, 1, 2, 3].pivot(
    namespace['P'][0, 2]) == [0, 3, 2, 1, 0, 3, 2, 1]
for zip_call in (
        lambda: namespace['P'][0, 1].zip(
            namespace['P'][2, 3], dtype=namespace['PGroup']),
        lambda: namespace['P'][*range(16)].zip(
            namespace['P'][*range(17)]),
        lambda: namespace['P'][*range(16)].deepzip(
            namespace['P'][*range(17)])):
    try:
        zip_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('zip helpers must enforce safe output bounds')
deepPattern = namespace['P'][0]
for _ in range(17):
    deepPattern = namespace['_FoxDotFinitePattern']([deepPattern])
try:
    deepPattern.deepzip(namespace['P'][1])
except ValueError as error:
    assert '16 nested pattern levels' in str(error)
else:
    raise AssertionError('deepzip must enforce the nested-depth bound')
for pattern_call in (
        lambda: namespace['P'][0, 1].map([1, 2]),
        lambda: namespace['P'][0].map({0: tuple(range(257))}),
        lambda: namespace['P'][*range(129)].map({
            value: (value, value + 1) for value in range(129)}),
        lambda: namespace['P'][0, 1].pivot(1.5),
        lambda: namespace['P'][*range(17)].pivot(
            namespace['P'][*range(17)])):
    try:
        pattern_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('map/pivot must validate inputs and enforce bounds')
for slice_call in (
        lambda: namespace['P'][0, 1].trim(1.5),
        lambda: namespace['P'][0, 1].ltrim(True)):
    try:
        slice_call()
    except TypeError:
        pass
    else:
        raise AssertionError('trim/ltrim require integer sizes')
for helper in (
        lambda: namespace['P']['x', 'o'].arp(namespace['P'][0, 2]),
        lambda: namespace['P'][0, 2].arp(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 1].arp(
            namespace['P'][*range(129)]),
        lambda: namespace['P'][0, 1].splice(),
        lambda: namespace['P'][0, 1].splice(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 1].splice(
            namespace['P']['x', 'o'])):
    try:
        helper()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('arp/splice should reject unsupported or unbounded input')
try:
    namespace['P'][2, None, 1].sort()
except TypeError:
    pass
else:
    raise AssertionError('sort should retain FoxDot ordering requirements')
deepPattern = namespace['P'][1]
for _ in range(18):
    deepPattern = namespace['_FoxDotFinitePattern']([deepPattern])
try:
    deepPattern.replace(1, 2)
except ValueError as error:
    assert '16 nested pattern levels' in str(error)
else:
    raise AssertionError('replace must enforce its nested-pattern depth bound')
for layer_call in (
        lambda: namespace['P'][0, 1].layer('layer'),
        lambda: namespace['P'][0, 1].layer('__class__'),
        lambda: namespace['P'][0, 1].layer(namespace['reverse']),
        lambda: namespace['P'][*range(255)].layer('stretch', 256)):
    try:
        layer_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('layer should enforce its transform allowlist and output bound')
try:
    namespace['P'][*range(86)].splice(
        namespace['P'][*range(86)], namespace['P'][*range(86)])
except ValueError as error:
    assert '256 items' in str(error)
else:
    raise AssertionError('splice must enforce the 256-item output cap')
shuffleSource = namespace['P'][(0, 4), None, 7, 2]
shuffleBefore = list(shuffleSource)
shuffleCopies = shuffleSource.shuffle(2)
assert len(shuffleCopies) == 2 * len(shuffleSource)
for chunkStart in (0, len(shuffleSource)):
    assert sorted(map(repr, shuffleCopies[chunkStart:chunkStart + len(shuffleSource)])) == \
        sorted(map(repr, shuffleBefore))
assert list(shuffleSource) == shuffleBefore
sampleShuffle = namespace['P']['x', 'o', ' ', '-'].shuffle(2)
assert len(sampleShuffle) == 8
assert sorted(sampleShuffle[:4]) == [' ', '-', 'o', 'x']
assert sorted(sampleShuffle[4:]) == [' ', '-', 'o', 'x']
assert namespace['P'][1, 2].shuffle(0) == []
trueSource = namespace['P'][
    namespace['P'][1, 2], 3, namespace['PGroup'](4, 5)]
trueShuffle = trueSource.true_shuffle(2)
assert len(trueShuffle) == 8
trueExpanded = [1, 2, 3, namespace['PGroup'](4, 5)]
for chunkStart in (0, 4):
    assert sorted(map(repr, trueShuffle[chunkStart:chunkStart + 4])) == \
        sorted(map(repr, trueExpanded))
shuffleSeedCounter = namespace['_dexter_shuffle_counter']
namespace['_dexter_shuffle_counter'] = 0
deterministicTrue = namespace['P'][0, 2, 4, 7].true_shuffle(2)
namespace['_dexter_shuffle_counter'] = 0
assert deterministicTrue == namespace['P'][0, 2, 4, 7].true_shuffle(2)
namespace['_dexter_shuffle_counter'] = 0
deterministicGroups = namespace['P'][0, 2, 4, 7].shufflets(2)
namespace['_dexter_shuffle_counter'] = 0
assert deterministicGroups == namespace['P'][0, 2, 4, 7].shufflets(2)
namespace['_dexter_shuffle_counter'] = shuffleSeedCounter
assert len(namespace['P'][1, 2, 3].true_shuffle(
    namespace['P'][1, 2])) == 9
trueSampleShuffle = namespace['P']['x', '-', 'o'].true_shuffle(2)
assert len(trueSampleShuffle) == 6
assert sorted(trueSampleShuffle[:3]) == ['-', 'o', 'x']
assert sorted(trueSampleShuffle[3:]) == ['-', 'o', 'x']
shuffleGroups = namespace['P'][0, 2, 4, 7].shufflets(3)
assert len(shuffleGroups) == 3
assert all(isinstance(group, namespace['_FoxDotChord'])
           and sorted(group) == [0, 2, 4, 7] for group in shuffleGroups)
assert len(namespace['P'][0, 2, 4].shufflets(
    namespace['P'][1, 2])) == 3
assert namespace['P'][0, 2].true_shuffle(0) == []
assert namespace['P'][0, 2].shufflets(0) == []
for random_method in (
        lambda: namespace['P'][0, 1].true_shuffle(True),
        lambda: namespace['P'][0, 1].true_shuffle(1.5),
        lambda: namespace['P'][0, 1].true_shuffle(
            namespace['P'][*range(257)]),
        lambda: namespace['P']['x', 'o'].shufflets(1),
        lambda: namespace['P'][0, None].shufflets(1),
        lambda: namespace['P'][*range(9)].shufflets(1),
        lambda: namespace['P'][0, 1].shufflets(-1)):
    try:
        random_method()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('true_shuffle/shufflets must enforce input bounds')
namespace['_dexter_shuffle_counter'] = shuffleCounterBeforeMethodTests
assert namespace['P'][1, 2, 3].palindrome() == [1, 2, 3, 3, 2, 1]
assert namespace['P'][1, namespace['P'][2, 3], 4].mirror() == [
    4, [3, 2], 1]
assert namespace['P'][1, 2, 3, 4].accum() == [0, 1, 3, 6]
assert namespace['P'][1, 2, 3].accum(5) == [0, 1, 3, 6, 7]
assert namespace['P'][1, 2].accum(namespace['P'][2, 3]) == [
    0, 1, 0, 1, 3]
assert namespace['P'][0, 1, 2, 3, 4].swap(2) == [1, 0, 3, 2, 4]
assert namespace['P'][0, 1, 2, 3, 4, 5].swap(
    namespace['P'][2, 3]) == [1, 0, 3, 2, 5, 4, 2, 1, 0, 5, 4, 3]
assert namespace['P']['x', namespace['P']['o', ' ']].mirror() == [
    [' ', 'o'], 'x']
for helper in (
        lambda: namespace['P'][1, 2].accum(257),
        lambda: namespace['P'][1, None, 2].accum(),
        lambda: namespace['P'][1, 2].swap(0),
        lambda: namespace['P'][1, 2].swap(1.5),
        lambda: namespace['P'][1, 2].stretch(True),
        lambda: namespace['P'][1, 2].stretch(1.5),
        lambda: namespace['P'][1, 2].stretch(257),
        lambda: namespace['P'][1, 2].stretch(namespace['P'][200, 100]),
        lambda: namespace['P'][1, 2].stretch(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][1, 2].loop(True),
        lambda: namespace['P'][1, 2].loop(-1),
        lambda: namespace['P'][1, 2].loop(1.5),
        lambda: namespace['P'][1, 2].loop(129),
        lambda: namespace['P'][1, 2].loop(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][1, 2].shuffle(True),
        lambda: namespace['P'][1, 2].shuffle(-1),
        lambda: namespace['P'][1, 2].shuffle(1.5),
        lambda: namespace['P'][1, 2].shuffle(129),
        lambda: namespace['P']['x', 'o'].invert(),
        lambda: namespace['P'][0, [1, 'x']].invert(),
        lambda: namespace['P'][0, (1, 2, 3, 4, 5, 6, 7, 8, 9)].invert()):
    try:
        helper()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('bounded pattern helper should reject invalid input')
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
assert namespace['P']['x', ' '] & namespace['P']['o', '-', ' '] == [
    ('x', 'o'), '-', 'x', 'o', ('x', '-'), ' ']
sampleChord = namespace['P']['x']
for symbol in 'abcdefg':
    sampleChord = sampleChord & namespace['P'][symbol]
assert sampleChord == [tuple('xabcdefg')]
try:
    sampleChord & namespace['P']['h']
except ValueError as error:
    assert 'at most 8 symbols' in str(error)
else:
    raise AssertionError('sample-symbol chord width must be capped at 8')
try:
    namespace['P'][*('x' * 16)] & namespace['P'][*('o' * 17)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('sample-symbol P zipping must cap LCM expansion at 256')
try:
    namespace['P']['x'] & namespace['P'][1]
except TypeError as error:
    assert 'cannot mix numeric' in str(error)
else:
    raise AssertionError('sample-symbol and numeric P patterns must not zip together')
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
assert namespace['PZip'](namespace['P']["x", " "],
                         namespace['P']["o", "-", " "]) == [
    ("x", "o"), "-", "x", "o", ("x", "-"), " "]
assert namespace['PZip'](
    namespace['PZip'](namespace['P']["x", "o"],
                      namespace['P']["-", " "]),
    namespace['P']["a"]) == [("x", "-", "a"), ("o", "a")]
assert namespace['PZip2'](
    namespace['P']["x", "o", "-"],
    namespace['P']["x", "-", " "],
    rule=lambda a, b: a != b) == [("o", "-"), "-"]
assert namespace['PZip'](
    [("x", "o", "a", "b")], [("c", "d", "e", "f")]) == [
        ("x", "o", "a", "b", "c", "d", "e", "f")]
assert namespace['PZip'](*[
    namespace['P'][symbol] for symbol in "abcdefgh"]) == [tuple("abcdefgh")]
assert namespace['PZip2'](
    namespace['P']["x"], namespace['P']["o"],
    rule=lambda a, b: False) == []
assert namespace['PRhythm']([2, (3, 8)]) == [2.0, (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([(3, 8)]) == [0.75, 0.75, 0.5]
assert namespace['PRhythm']([2, (3, 8, 1, 0.25)]) == [
    2.0, (2.0, 0.75, 1.25)]
assert namespace['PRhythm'](namespace['P'][1, (3, 8)]) == [
    1.0, (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([1, [0.25, 0.25]]) == [
    1.0, (0.5, 0.25)]
assert namespace['PRhythm']([0.75, [0.25, 0.25], (3, 8)]) == [
    0.75, (0.5, 0.25), (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([0.5, [0.25, [0.125, 0.125]]]) == [
    0.5, (0.5, 0.25, 0.375)]
deepRhythm = [1]
for _ in range(9):
    deepRhythm = [deepRhythm]
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
        lambda: namespace['PZip']([1], [tuple(range(9))]),
        lambda: namespace['PZip'](["x"] * 16, ["o"] * 17),
        lambda: namespace['PZip'](*[[symbol] for symbol in "abcdefghi"]),
        lambda: namespace['PZip']([("x", "o", "a", "b")], [("c", "d", "e", "f", "g")]),
        lambda: namespace['PZip']([1], namespace['P']["x"]),
        lambda: namespace['PZip'](["x", 1], ["o"]),
        lambda: namespace['PZip2']([1] * 16, [2] * 17),
        lambda: namespace['PZip2'](["x"] * 16, ["o"] * 17),
        lambda: namespace['PZip2']([1], [2], rule=0),
        lambda: namespace['PRhythm']([]),
        lambda: namespace['PRhythm']([0]),
        lambda: namespace['PRhythm']([True]),
        lambda: namespace['PRhythm']([1, (3,)]),
        lambda: namespace['PRhythm']([1, (3.0, 8)]),
        lambda: namespace['PRhythm']([1, (3, 64)]),
        lambda: namespace['PRhythm']([[9, 1]]),
        lambda: namespace['PRhythm']([[4096, 1]]),
        lambda: namespace['PRhythm']([[1] * 257]),
        lambda: namespace['PRhythm']([deepRhythm]),
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

def run_script(script_text, include_events=False, include_effects=False, include_filters=False, include_decay=False, include_mutes=False, include_modulations=False, include_samples=False, include_sample_controls=False, include_player_effects=False, include_synth_controls=False, include_effect_control_vars=False, include_group_controls=False, include_tempo=False, user_synth_names=()):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper = make_wrapper(source_path, result_path, user_synth_names)
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
        player_effect_rows = [line.split('\t') for line in lines if line.startswith('A\t')]
        synth_control_rows = [line.split('\t') for line in lines if line.startswith('S\t')]
        effect_control_var_rows = [line.split('\t') for line in lines if line.startswith('X\t')]
        filter_rows = [line.split('\t') for line in lines if line.startswith('F\t')]
        modulation_rows = [line.split('\t') for line in lines if line.startswith('G\t')]
        decay_rows = [line.split('\t') for line in lines if line.startswith('E\t')]
        mute_rows = [line.split('\t') for line in lines if line.startswith('M\t')]
        if include_tempo:
            return player_rows, [line.split('\t') for line in lines if line.startswith('T\t')]
        if include_group_controls:
            return (player_rows, effect_rows, filter_rows, decay_rows,
                    modulation_rows)
        if include_sample_controls:
            return (player_rows, [line.split('\t') for line in lines if line.startswith('B\t')],
                    [line.split('\t') for line in lines if line.startswith('Q\t')])
        if include_player_effects and include_synth_controls:
            if include_effect_control_vars:
                return (player_rows, player_effect_rows, synth_control_rows,
                        effect_control_var_rows)
            return player_rows, player_effect_rows, synth_control_rows
        if include_effect_control_vars:
            return player_rows, effect_control_var_rows
        if include_player_effects:
            return player_rows, player_effect_rows
        if include_synth_controls:
            return player_rows, synth_control_rows
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

def run_script_with_schedule_seed(script_text, seed, **options):
    global wrapper_cpp
    original = wrapper_cpp
    marker = 'int(time.monotonic_ns() & 0xffffffff)'
    if marker not in original:
        raise AssertionError('Could not find the runtime shuffle seed in wrapper')
    wrapper_cpp = original.replace(marker, str(int(seed)))
    try:
        options.setdefault('include_events', True)
        return run_script(script_text, **options)
    finally:
        wrapper_cpp = original

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

def run_error_script(script_text, user_synth_names=()):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper_globals = {'__name__': '__main__'}
        exec(make_wrapper(source_path, result_path, user_synth_names),
             wrapper_globals, wrapper_globals)
        return open(result_path, encoding='utf-8').read()

def run_example(name, user_synth_names=(), include_player_effects=False,
                include_synth_controls=False, include_effect_control_vars=False):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read(),
                      user_synth_names=user_synth_names,
                      include_player_effects=include_player_effects,
                      include_synth_controls=include_synth_controls,
                      include_effect_control_vars=include_effect_control_vars)

sampleSetupPath = os.path.join(sys.argv[2], 'SAMPLES-SETUP.html')
sampleSetupHtml = open(sampleSetupPath, encoding='utf-8').read()
assert 'Portable/Samples' in sampleSetupHtml and 'properly licensed WAV' in sampleSetupHtml

userSynthBinaryPath = os.path.join(sys.argv[2], 'SynthDefs', 'CustomPluck.scsyndef')
userSynthBinary = open(userSynthBinaryPath, 'rb').read()
assert b'CustomPluck' in userSynthBinary
assert b'DexterPluck' not in userSynthBinary
assert not os.path.exists(os.path.join(sys.argv[2], 'SynthDefs', 'DexterPluck.scsyndef'))
userSynthRows = run_example('94-user-synthdef-loader.py', ('CustomPluck',))
assert [int(row[6]) for row in userSynthRows] == [58, 58]
missingUserSynth = run_error_script(
    'p1 >> userSynth("NotInstalled", [0, 2], dur=0.5)\n')
assert 'not a loaded Portable/SynthDefs entry' in missingUserSynth

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

def run_compatibility_check(script_text, user_synth_names=()):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        report_path = os.path.join(directory, 'report.txt')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        namespace = {'_compat_source_path': source_path, '_compat_report_path': report_path,
                     '_USER_SYNTH_NAMES': tuple(user_synth_names)}
        exec(compile(compatibility_checker, '<embedded compatibility checker>', 'exec'), namespace, namespace)
        return open(report_path, encoding='utf-8').read()

def read_synthdef_manifest(data, validate_builtin_controls=True, return_controls=False):
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
    controls = {}
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
        if version == 2 and validate_builtin_controls:
            expected_controls = (
                'out freq rate sus unused4 unused5 wetbus wetgain wetcutoff trigger '
                'amp vib depth lforate lfowidth rq fmod blur cutoff atk rel beat_dur'
            ).split()
            if name == 'DexterSample':
                expected_controls.extend(('buf', 'start', 'end', 'loop'))
            assert parameter_names == {key: index for index, key in enumerate(expected_controls)}, (
                name, 'C++ control-number ABI mismatch', parameter_names)
        controls[name] = parameter_names
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
    return (version, graphs, controls) if return_controls else (version, graphs)


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
    sample_version, sample_graphs, sample_controls = read_synthdef_manifest(
        sample_bytes, return_controls=True)
    assert sample_version == 2 and set(sample_graphs) == {'DexterSample'}
    assert any(ugen[0] == 'PlayBuf' for ugen in sample_graphs['DexterSample'])
    assert any(ugen[0] == 'Phasor' for ugen in sample_graphs['DexterSample'])
    assert any(ugen[0] == 'BufRd' for ugen in sample_graphs['DexterSample'])
    assert sample_controls['DexterSample']['buf'] == 22
    assert [sample_controls['DexterSample'][name] for name in ('start', 'end', 'loop')] == [23, 24, 25]
    user_def_path = os.path.join(plugin_dir, 'Portable', 'SynthDefs', 'CustomPluck.scsyndef')
    user_version, user_graphs, user_controls = read_synthdef_manifest(
        open(user_def_path, 'rb').read(), validate_builtin_controls=False,
        return_controls=True)
    assert user_version == 2 and set(user_graphs) == {'CustomPluck'}
    assert user_controls['CustomPluck'] == {
        'out': 0, 'freq': 1, 'amp': 2, 'gate': 3, 'atk': 4, 'rel': 5}
    assert any(ugen[0] == 'Out' for ugen in user_graphs['CustomPluck'])
    assert any(ugen[0] == 'SinOsc' for ugen in user_graphs['CustomPluck'])
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
        r'_DEXTER_BUILTIN_SYNTHS\s*=\s*\((.*?)\)', api, re.S)
    assert synth_names_match, 'could not find the Python synth selector'
    synth_names = ast.literal_eval('(' + synth_names_match.group(1) + ')')
    assert len(synth_names) == len(registered_graphs) + 3, \
        'four base waveforms share DexterTone; every other player synth needs one graph'
    reserved_names_match = re.search(
        r'kEmbeddedFoxDotSynthNames\s*=\s*\{(.*?)\};', processor_source, re.S)
    assert reserved_names_match, 'could not find the C++ reserved synth-name list'
    reserved_names = tuple(re.findall(r'"([^"]+)"', reserved_names_match.group(1)))
    assert reserved_names == synth_names, \
        'C++ must reject every filename that collides with a built-in FoxDot synth'
    assert ('len(_DEXTER_BUILTIN_SYNTHS) +' in api and
            '_dexter_user_synth_names.index(pattern.synth)' in api)
    assert 'kVoiceSlotsPerPlayer = kMaxPolyphony * 2 + 1' in processor_source
    assert 'kPlayerCount * kVoiceSlotsPerPlayer' in processor_source
    assert 'kAudioVoiceCount * kMaxCachedSynthGraphsPerVoice' in processor_source
    assert 'kPlayerCount * (kReverbGraphCount + 1) + 1' in processor_source
    assert 'kAirwindowsInputBusBase' in processor_source
    assert 'playerEffectGraphs_[player]' in processor_source
    assert 'Graph_Calc(graph)' in processor_source
    assert 'kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity' in processor_source
    assert 'static_assert(kWorldNodeCapacity >= 2048' in processor_source
    assert 'scPoolCapacity(kSynthGraphCount + kReverbGraphCount)' in processor_source
    assert 'scPoolCapacity(kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity)' in processor_source
    assert 'prepareSynthGraphs(updates, updateMask, userSynthNames,' in processor_source
    assert 'static bool hasOutUnit(GraphDef* definition, std::size_t channels)' in processor_source
    assert 'unit.mNumInputs == channels + 1' in processor_source
    assert 'needs an Out UGen that writes exactly one mono signal' in processor_source
    assert 'Graph_New(world_, synthDefinitions_[graphIndex]' in processor_source
    assert 'name.size() > kSCNodeDefNameByteLen - 1' in processor_source, \
        'user SynthDef identifiers must allow SuperCollider names up to 255 bytes'
    assert 'std::array<int32, kSCNodeDefNameLen> nameKey{}' in processor_source
    assert 'World_GetGraphDef(world_, nameKey.data())' in processor_source
    assert 'bool startImmediately = false' in processor_source
    assert 'if (!startImmediately)\n            Node_SetRun(&created->mNode, 0)' in processor_source
    assert 'graphError, true);' in processor_source and 'createUserSynthGraph' in processor_source
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
        r'Graph\* graphForSynthIndex\(AudioVoice& voice, int synthIndex\)\s*\{(.*?)\n    \}',
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
assert len(legacy) == 1 and len(legacy[0]) == 23
namespace['tempo'](10000)
assert namespace['_dexter_bpm'] == 10000.0
namespace['tempo'](0.001)
assert namespace['_dexter_bpm'] == 0.001
for badTempo in (0, -1, 0.0009, float('inf'), float('-inf'), float('nan')):
    try:
        namespace['tempo'](badTempo)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('tempo must reject out-of-range or non-finite values')
namespace['tempo'](120)
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
assert len(example7) == 2 and all(len(row) == 23 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.0'
example9 = run_example('09-foxdot-core.py')
assert len(example9) == 2 and all(len(row) == 23 for row in example9)
foxSaw, foxPulse = example9
assert foxSaw[1] == '0' and foxSaw[3] == '96.0' and foxSaw[6] == '2'
assert abs(float(foxSaw[7]) - 0.7) < 1e-6 and abs(float(foxSaw[8]) + 0.35) < 1e-6
assert foxPulse[1] == '1' and foxPulse[3] == '96.0' and foxPulse[6] == '3'
assert abs(float(foxPulse[7]) - 0.25) < 1e-6 and abs(float(foxPulse[8]) - 0.35) < 1e-6
example10Rows = run_example('10-pattern-transformations.py')
assert len(example10Rows) == 2 and all(len(row) == 23 for row in example10Rows)
assert example10Rows[0][5] == '7' and example10Rows[0][6] == '2'
assert example10Rows[1][5] == '4' and example10Rows[1][6] == '3'
example11Rows = run_example('11-foxdot-pattern-literals.py')
assert len(example11Rows) == 2 and all(len(row) == 23 for row in example11Rows)
assert example11Rows[0][5] == '7' and example11Rows[1][5] == '2'
example12Rows = run_example('12-pattern-arithmetic.py')
assert len(example12Rows) == 2 and all(len(row) == 23 for row in example12Rows)
assert example12Rows[0][5] == '5' and example12Rows[1][5] == '5'
example13Rows = run_example('13-pattern-methods.py')
assert len(example13Rows) == 5 and all(len(row) == 23 for row in example13Rows)
assert [row[5] for row in example13Rows] == ['8', '5', '4', '4', '4']
example14Rows = run_example('14-pattern-composition.py')
assert len(example14Rows) == 2 and all(len(row) == 23 for row in example14Rows)
assert [row[5] for row in example14Rows] == ['5', '7']
example15Rows = run_example('15-pattern-arithmetic-operators.py')
assert len(example15Rows) == 4 and all(len(row) == 23 for row in example15Rows)
assert [row[5] for row in example15Rows] == ['4', '2', '4', '4']
example16Rows = run_example('16-bounded-pattern-slices.py')
assert len(example16Rows) == 3 and all(len(row) == 23 for row in example16Rows)
assert [row[5] for row in example16Rows] == ['8', '4', '2']
example17Rows = run_example('17-live-player-edits.py')
assert len(example17Rows) == 2 and [row[1] for row in example17Rows] == ['0', '1']
example18Rows = run_example('18-ui-mouse-controls.py')
assert len(example18Rows) == 2 and all(len(row) == 23 for row in example18Rows)
assert example18Rows[0][12] == '1' and example18Rows[0][13] == '0'
assert example18Rows[1][12] == '0' and example18Rows[1][13] == '1'
envelopeRows = run_example('19-envelope-controls.py')
assert len(envelopeRows) == 2 and all(len(row) == 23 for row in envelopeRows)
assert float(envelopeRows[0][14]) == 0.005 and float(envelopeRows[0][15]) == 0.2
assert float(envelopeRows[1][14]) == 0.04 and float(envelopeRows[1][15]) == 0.1
timevarRows = run_example('20-timevar-controls.py')
assert len(timevarRows) == 2 and all(len(row) == 23 for row in timevarRows)
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
assert len(effectRows) == 1 and len(effectRows[0]) == 23 and len(effectEvents) == 4
assert effectRows[0][19] == 'v:0.1,0.5:1.0,1.0'
assert effectRows[0][20] == 'l:0.1,0.5:1.0,1.0'
assert abs(float(effectEvents[0][7]) - 0.0) < 1e-9
assert abs(float(effectEvents[1][7])) < 1e-9
assert abs(float(effectEvents[2][7])) < 1e-9
assert abs(float(effectEvents[0][5]) - 0.01) < 1e-9 and abs(float(effectEvents[0][6]) - 0.02) < 1e-9
assert abs(float(effectEvents[1][5]) - 0.01) < 1e-9 and abs(float(effectEvents[1][6]) - 0.06) < 1e-9
assert abs(float(effectEvents[2][5]) - 0.03) < 1e-9 and abs(float(effectEvents[2][6]) - 0.1) < 1e-9
effectExample = run_example('22-timevar-effects-envelopes.py')
assert len(effectExample) == 2 and all(len(row) == 23 for row in effectExample)
reverbRows, reverbEffects = run_script(open(os.path.join(sys.argv[2], '23-reverb-algorithms.py'), encoding='utf-8').read(), include_effects=True)
assert len(reverbRows) == 3 and len(reverbEffects) == 3
assert [int(row[2]) for row in reverbEffects] == [1, 2, 3]
assert [float(row[4]) for row in reverbEffects] == [0.65, 0.75, 0.8]
assert [float(row[7]) for row in reverbEffects] == [15.0, 15.0, 25.0]
assert all(abs(float(row[3]) - expected) < 1e-6 for row, expected in zip(reverbEffects, [0.2, 0.3, 0.25]))
filterRows, filterControls = run_script('p1 >> saw([0, 2, 4, 6], lpf=linvar([300, 3000], [2, 2]), lpr=var([0.35, 0.7], [2, 2]), hpf=var([0, 150], [2, 2]), hpr=0.8, bpf=linvar([500, 1200], [2, 2]), bpr=0.5)\n', include_filters=True)
assert len(filterRows) == 1 and len(filterRows[0]) == 23
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
assert 'No unsupported constructs detected' in run_compatibility_check(
    'Clock.bpm = 120\nClock.swing()\np1 >> triangle([0, 2], dur=0.5)\n')
assert 'Clock.swing accepts one optional amount' in run_compatibility_check(
    'Clock.swing(0.1, 0.2)\n')
assert 'Clock.swing accepts only the amount keyword' in run_compatibility_check(
    'Clock.swing(foo=0.1)\n')
assert 'Clock.swing is a method; call Clock.swing(amount)' in run_compatibility_check(
    'Clock.swing = 0.1\n')
for tempo_function, token in (('var', 'v'), ('linvar', 'l')):
    tempo_source = ('Clock.bpm = ' + tempo_function + '([80, 120], [8, 4])\n'
                    'p1 >> sine([0, 2], dur=0.5)\n'
                    'p2 >> play("x-o-", dur=0.5)\n')
    assert 'No unsupported constructs detected' in run_compatibility_check(tempo_source)
    tempo_rows, tempo_controls = run_script(tempo_source, include_tempo=True)
    assert len(tempo_rows) == 2
    assert all(float(row[3]) == 80.0 for row in tempo_rows)
    assert [row[2] for row in tempo_controls] == [
        token + ':80.0,120.0:8.0,4.0'] * 2
# The final global setting applies even when it appears below the players.
tempo_rows, tempo_controls = run_script(
    'p1 >> sine([0])\nClock.bpm = var([90, 135], 8)\n', include_tempo=True)
assert float(tempo_rows[0][3]) == 90.0
assert tempo_controls[0][2] == 'v:90.0,135.0:8.0,8.0'
tempo_rows, tempo_controls = run_script(
    'Clock.bpm = var([90, 135], 8)\nClock.bpm = 110\np1 >> sine([0])\n',
    include_tempo=True)
assert float(tempo_rows[0][3]) == 110.0 and tempo_controls[0][2] == '-'
for invalid_tempo in ('var([0, 120], 4)', 'linvar([-1, 120], 4)',
                      'var([120, float("inf")], 4)', '0', '-10'):
    assert 'tempo must be finite and at least 0.001 BPM' in run_error_script(
        'Clock.bpm = ' + invalid_tempo + '\np1 >> sine([0])\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.bpm = var([80, 400], 8)\nClock.swing(0.2)\np1 >> sine([0])\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.swing(0.1)\nClock.bpm = linvar([80, 400], 8)\np1 >> sine([0])\n')
swingRows = run_script('Clock.bpm = 120\nClock.swing()\np1 >> triangle([0, 2], dur=0.5)\n')
assert len(swingRows[0]) == 23 and abs(float(swingRows[0][22]) - 0.1) < 1e-9
swingAfterPlayerRows = run_script(
    'p1 >> triangle([0, 2], dur=0.5)\nClock.swing(0.05)\n')
assert abs(float(swingAfterPlayerRows[0][22]) - 0.05) < 1e-9
assert abs(float(run_script(
    'Clock.swing(0.1)\nClock.swing(0)\np1 >> triangle([0], dur=1)\n')[0][22])) < 1e-9
assert '0.49 beats or more' in run_error_script(
    'Clock.bpm = 200\nClock.swing(0.1)\np1 >> triangle([0], dur=1)\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.swing(0.1)\nClock.bpm = 400\np1 >> triangle([0], dur=1)\n')
assert 'must be finite' in run_error_script(
    "Clock.swing(float('nan'))\np1 >> triangle([0], dur=1)\n")
rootRows, rootEvents = run_script(open(os.path.join(sys.argv[2], '27-root-and-scale-controls.py'), encoding='utf-8').read(), include_events=True)
assert len(rootRows) == 3 and [row[1] for row in rootRows] == ['0', '1', '2']
assert [int(event[3]) for event in rootEvents] == [62, 65, 69, 72, 55, 58, 62, 57, 60, 63, 63, 67, 70, 74]
assert 'No unsupported constructs detected' in run_compatibility_check('Scale.default = "minor"\nRoot.default.set("Eb")\np1 >> saw([0, 2, 4], root=7)\n')
assert 'Root.default only supports .set(root)' in run_compatibility_check('Root.default.notes\n')
rootTimevarText = (
    'Scale.default = Scale.major\n'
    'Root.default.set(var([0, 7], [2, 2]))\n'
    'Scale.default.set(Scale.major)\n'
    'p1 >> triangle(P[0, 2, 4, 6], dur=1, oct=5)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(rootTimevarText)
rootTimevarRows, rootTimevarEvents = run_script(
    rootTimevarText, include_events=True)
assert len(rootTimevarRows) == 1
assert [int(event[3]) for event in rootTimevarEvents] == [
    60, 64, 74, 78]
rootLinvarRows, rootLinvarEvents = run_script(
    'Root.default.set(linvar([0, 4], [4, 4]))\n'
    'p1 >> triangle(P[0, 0, 0, 0], dur=1, oct=5)\n',
    include_events=True)
assert len(rootLinvarRows) == 1
assert [int(event[3]) for event in rootLinvarEvents] == [60, 61, 62, 63]
rootOverrideRows, rootOverrideEvents = run_script(
    'Root.default.set(var([0, 7], [2, 2]))\n'
    'p1 >> triangle(P[0, 0], dur=1, oct=5, root=2)\n'
    'p2 >> triangle(P[0, 0, 0, 0], dur=1, oct=5)\n',
    include_events=True)
assert len(rootOverrideRows) == 2
assert [int(event[3]) for event in rootOverrideEvents[:2]] == [62, 62]
assert [int(event[3]) for event in rootOverrideEvents[2:]] == [60, 60, 67, 67]
assert 'pitch classes 0..11 or MIDI notes 12..127' in run_error_script(
    'Root.default.set(var([0, 60], [2, 2]))\n'
    'p1 >> triangle([0], dur=1)\n')
assert 'Root.default.set requires' in run_error_script(
    'Root.default.set(None)\np1 >> triangle([0], dur=1)\n')
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
assert 'update.synthRate' in parse_source and 'kEmbeddedSynthCount + userSynthCount' in parse_source
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
assert len(example8) == 2 and all(len(row) == 23 for row in example8)
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
generatorCompatibilityReport = run_compatibility_check(
    'p1 >> saw(PSq(0, 2, 5).norm(), dur=0.25)\n'
    'p2 >> play(PEuclid2(3, 8, "-", "x"), dur=0.25, sample=0)\n'
    'p3 >> saw(PRange(0, 8, 2).undup(), dur=0.5)\n')
assert 'No unsupported constructs detected' in generatorCompatibilityReport, (
    generatorCompatibilityReport)
for unsupported_pattern_method in ('sample', 'compress', 'transpose'):
    for pattern_expression in (
            'P[0, 2, 4]',
            'notes = P[0, 2, 4]\nnotes'):
        patternMethodReport = run_compatibility_check(
            pattern_expression + '.' + unsupported_pattern_method +
            '()\np1 >> saw([0])\n')
        assert 'P pattern method' in patternMethodReport, patternMethodReport
        assert unsupported_pattern_method in patternMethodReport, patternMethodReport
for malformed_generator_call, expected_diagnostic in (
        ('PRange()', 'requires start'),
        ('PStep(4)', 'requires value'),
        ('PEuclid(3)', 'requires k'),
        ('PEuclid2(3, 8, "-", "x", 1)', 'at most 4 positional')):
    malformedGeneratorReport = run_compatibility_check(
        'p1 >> saw(' + malformed_generator_call + ')\n')
    assert expected_diagnostic in malformedGeneratorReport, (
        malformedGeneratorReport)
loadedUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("CustomPluck", P[0, 2, 4], dur=0.5, amp=0.3)\n',
    ('CustomPluck',))
assert 'No unsupported constructs detected' in loadedUserSynthReport, loadedUserSynthReport
mappedUserSynthControlReport = run_compatibility_check(
    'p1 >> userSynth("CustomPluck", [0, 2], vib=0.5)\n',
    ('CustomPluck',))
assert 'No unsupported constructs detected' in mappedUserSynthControlReport, mappedUserSynthControlReport
missingUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("NotInstalled", [0, 2], dur=0.5)\n')
assert 'not present in Portable/SynthDefs' in missingUserSynthReport
unsupported_report = run_compatibility_check('from FoxDot import *\nClock.swing = 0.2\np9 >> bass([0, 2], dur=0.5)\nplay("x-o-")\nP["x-o"]\nP[0:]\nP[0:257]\nP[0:8:0]\nP[start:8]\nP[0] | ["x-o"]\n')
for phrase in ('Line 1:', 'Line 2:', 'Line 5:', 'P sample-symbol literals', 'finite stop', '256 generated items', 'step cannot be zero', 'integer-literal bounds', 'Pattern | and & do not accept sample strings'):
    assert phrase.lower() in unsupported_report.lower(), (phrase, unsupported_report)
syntax_report = run_compatibility_check('p1 >> saw([0])\nif :\n')
assert 'Python syntax error at column' in syntax_report and 'Line 2:' in syntax_report, syntax_report
for filename in os.listdir(sys.argv[2]):
    if filename.endswith('.py'):
        example_names = (
            ('CustomPluck',) if filename == '94-user-synthdef-loader.py' else
            ('AirwindowsTapeDelay2',)
            if filename == '103-sample-player-airwindows-delay.py' else
            ('AirwindowsEchojam',)
            if filename == '104-sample-player-airwindows-echojam.py' else
            ('AirwindowsTapeDelay2', 'AirwindowsEchojam')
            if filename == '105-airwindows-timevar-controls.py' else ())
        example_report = run_compatibility_check(
            open(os.path.join(sys.argv[2], filename), encoding='utf-8').read(),
            example_names)
        assert 'No unsupported constructs detected' in example_report, (filename, example_report)
for scale_name in namespace['_SCALES']:
    namespace['Scale'].default.set(scale_name)
    assert namespace['Scale'].__dict__[scale_name] == scale_name
example69 = run_example('69-composed-star-and-karp.py')
assert [int(row[6]) for row in example69 if row[2] == '0'] == [55, 56, 0]
example70 = run_example('70-portable-sample-beat.py')
assert int(next(row for row in example70 if row[2] == '0')[6]) == 57
example141Text = open(os.path.join(
    sys.argv[2], '141-finite-pattern-generators.py'), encoding='utf-8').read()
example141, example141Samples, example141Events = run_script(
    example141Text, include_samples=True, include_events=True)
assert {int(row[1]) for row in example141 if row[2] == '0'} == {0, 1, 2, 3}
assert len(example141Samples) == 3
assert len(example141Events) == 19
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
example100 = run_example('100-sample-loop-and-slicing.py')
assert [int(row[6]) for row in example100 if row[2] == '0'] == [57, 57]
example101 = run_example('101-patterned-sample-slices.py')
assert [int(row[6]) for row in example101 if row[2] == '0'] == [57, 57]
example102 = run_example('102-random-pattern-generators.py')
assert [int(row[5]) for row in example102 if row[0] == 'P'] == [16, 16]
example103, example103EffectRows = run_example(
    '103-sample-player-airwindows-delay.py', ('AirwindowsTapeDelay2',),
    include_player_effects=True)
assert [int(row[5]) for row in example103 if row[0] == 'P'] == [28, 6]
assert example103EffectRows == [['A', '0', '0'], ['A', '1', '-1']]
example103, example103ControlRows = run_example(
    '103-sample-player-airwindows-delay.py', ('AirwindowsTapeDelay2',),
    include_synth_controls=True)
example103TapeControls = next(row for row in example103ControlRows
                              if row[1] == '0')
assert [float(value) for value in example103TapeControls[2:]] == [
    0.72, 0.64, 0.35, 0.42, 0.18, 0.0, 0.76]
example104, example104EffectRows, example104ControlRows = run_example(
    '104-sample-player-airwindows-echojam.py', ('AirwindowsEchojam',),
    include_player_effects=True, include_synth_controls=True)
assert [int(row[5]) for row in example104 if row[0] == 'P'] == [28, 6]
assert example104EffectRows == [['A', '0', '0'], ['A', '1', '-1']]
example104EchojamControls = next(row for row in example104ControlRows
                                 if row[1] == '0')
assert [float(value) for value in example104EchojamControls[2:]] == [
    0.62, 0.72, 0.38, 0.5, 0.5, 0.0, 0.82]
modulatedEffect = run_script(
    'p1 >> play("x", effect="AirwindowsTapeDelay2", '
    'fx_vib=var([0.2, 0.8], [2, 3]), '
    'fx_depth=linvar([0.1, 0.9], 8), fx_blur=0.6)\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('AirwindowsTapeDelay2',))
modulatedControls = next(row for row in modulatedEffect[3] if row[1] == '0')
assert modulatedControls[2:] == [
    'v:0.2,0.8:2.0,3.0', 'l:0.1,0.9:8.0,8.0',
    '-', '-', '-', '-', '-']
modulatedEchojam = run_script(
    'p1 >> play("x", effect="AirwindowsEchojam", '
    'fx_lforate=var([0.15, 0.7], 4), '
    'fx_blur=linvar([0.2, 0.8], [2, 6]))\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('AirwindowsEchojam',))
echojamControlVars = next(row for row in modulatedEchojam[3] if row[1] == '0')
assert echojamControlVars[2:] == [
    '-', '-', 'v:0.15,0.7:4.0,4.0', '-', '-', '-', 'l:0.2,0.8:2.0,6.0']
badTapeControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsTapeDelay2", fx_depth=1.1)\n',
    ('AirwindowsTapeDelay2',))
assert 'fx_depth must be a finite number or var/linvar from 0 to 1' in badTapeControl
unsupportedEchojamControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", fx_rq=0.4)\n',
    ('AirwindowsEchojam',))
assert 'fx_rq is not a control on AirwindowsEchojam' in unsupportedEchojamControl
timeVaryingEffectControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", fx_depth=var([0.2, 0.8]))\n',
    ('AirwindowsEchojam',))
assert not timeVaryingEffectControl.startswith('ERROR'), timeVaryingEffectControl
invalidTimeVaryingEffectControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", '
    'fx_depth=linvar([0.2, 1.2]))\n', ('AirwindowsEchojam',))
assert 'time-var values must be between 0 and 1' in invalidTimeVaryingEffectControl
example105, example105Effects, example105Controls, example105ControlVars = run_example(
    '105-airwindows-timevar-controls.py',
    ('AirwindowsTapeDelay2', 'AirwindowsEchojam'),
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True)
assert example105Effects == [['A', '0', '0'], ['A', '1', '1'], ['A', '2', '-1']]
assert len([row for row in example105ControlVars if row[1] == '0']) == 1
assert len([row for row in example105ControlVars if row[1] == '1']) == 1
assert all(any(token[0] in 'vl' for token in row[2:])
           for row in example105ControlVars if row[1] in ('0', '1'))
orphanEffectControl = run_error_script(
    'p1 >> play("x", fx_blur=0.4)\n')
assert 'fx_* controls require effect=' in orphanEffectControl
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x-o-", effect="AirwindowsTapeDelay2")\n',
    ('AirwindowsTapeDelay2',))
assert 'requires the matching Airwindows SynthDef' in run_compatibility_check(
    'p1 >> play("x-o-", effect="AirwindowsTapeDelay2")\n')
assert 'supports AirwindowsTapeDelay2 or AirwindowsEchojam' in run_compatibility_check(
    'p1 >> play("x-o-", effect="NotAnEffect")\n',
    ('NotAnEffect',))
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
pvarNoteRows, pvarNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2], P[4, 6]], [1, 1]), '
    'dur=0.5, sus=0.4, oct=4)\n', include_events=True)
assert int(pvarNoteRows[0][5]) == 4 and float(pvarNoteRows[0][4]) == 2.0
assert [int(row[3]) for row in pvarNoteEvents] == [48, 52, 55, 59]
pvarSampleRows, pvarSampleRequests, pvarSampleEvents = run_script(
    'p1 >> play(Pvar(["x-o-", "o--x"], [2, 2]), '
    'dur=0.5, sample=0)\n', include_samples=True, include_events=True)
assert int(pvarSampleRows[0][5]) == 8
assert [int(row[1]) for row in pvarSampleRequests] == [
    120, 45, 111, 45, 111, 45, 45, 120]
assert [float(row[1]) for row in pvarSampleEvents] == [
    0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5]
pvarVariableNoteRows, pvarVariableNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2, 4, 6], P[7, 5, 3, 1]], [3, 3]), '
    'dur=P[0.5, 1.0], sus=0.4, oct=4)\n', include_events=True)
assert [float(row[1]) for row in pvarVariableNoteEvents] == [
    0.0, 0.5, 1.5, 2.0, 3.0, 3.5, 4.5, 5.0]
pvarVariableSampleRows, pvarVariableSampleRequests = run_script(
    'p1 >> play(Pvar(["x---", "--xo"], [3, 3]), '
    'dur=P[0.5, 1.0], sample=0)\n', include_samples=True)
assert int(pvarVariableSampleRows[0][5]) == 8
pvarVariableExample = run_example('137-pvar-phrase-switching.py')
assert len(pvarVariableExample) == 2
assert all(float(row[4]) == 12.0 for row in pvarVariableExample)
assert int(pvarVariableExample[0][5]) == 16
assert int(pvarVariableExample[1][5]) > 0
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '137-pvar-phrase-switching.py'), encoding='utf-8').read())
pvarTimevarNoteRows, pvarTimevarNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2, 4], P[7, 5, 3]], [2, 2]), '
    'dur=var([0.5, 1.0], [1, 1]), sus=0.4, oct=4)\n',
    include_events=True)
assert [float(row[1]) for row in pvarTimevarNoteEvents] == [
    0.0, 0.5, 1.0, 2.0, 2.5, 3.0]
pvarTimevarSampleRows, pvarTimevarSampleRequests, pvarTimevarSampleEvents = (
    run_script(
        'p1 >> play(Pvar(["x-", "-o"], [1, 1]), '
        'dur=linvar([0.25, 1.25], [0.5, 0.5]), sample=0)\n',
        include_samples=True, include_events=True))
assert [float(row[1]) for row in pvarTimevarSampleEvents] == [
    0.0, 0.25, 1.0, 1.25]
assert [int(request[1]) for request in pvarTimevarSampleRequests] == [
    ord('x'), ord('-'), ord('-'), ord('o')]
pvarTimevarExample = run_example('138-pvar-timevar-durations.py')
assert len(pvarTimevarExample) == 2
assert float(pvarTimevarExample[0][4]) == 4.0
assert float(pvarTimevarExample[1][4]) == 2.0
assert int(pvarTimevarExample[0][5]) == 6
assert int(pvarTimevarExample[1][5]) == 4
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '138-pvar-timevar-durations.py'),
        encoding='utf-8').read())
pvarGroupedRows, pvarGroupedEvents = run_script(
    'p1 >> triangle(Pvar([P[0, 2], P[7, 5]], [3, 3]), '
    'dur=PRhythm([1, (3, 8)]), sus=0.2, oct=4)\n',
    include_events=True)
assert len(pvarGroupedRows) == 1 and len(pvarGroupedEvents) == 8
assert [round(float(row[1]) + float(row[7]), 6)
        for row in pvarGroupedEvents] == [
    0.0, 1.0, 1.75, 2.5, 3.0, 4.0, 4.75, 5.5]
pvarGroupedExample = run_example('139-pvar-grouped-rhythm.py')
assert len(pvarGroupedExample) == 2
assert int(pvarGroupedExample[0][5]) == 8
assert int(pvarGroupedExample[1][5]) == 8
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '139-pvar-grouped-rhythm.py'),
        encoding='utf-8').read())
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> pluck(Pvar([P[0, 2], P[4, 6]], [1, 1]), dur=0.5)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(Pvar(["x-o-", "o--x"], [2, 2]), dur=0.5)\n')
assert 'Pvar accepts only patterns and dur' in run_compatibility_check(
    'p1 >> pluck(Pvar(P[0, 2], invalid=True))\n')
assert 'Pvar cannot mix numeric note patterns with sample-symbol patterns' in (
    run_compatibility_check(
        'p1 >> pluck(Pvar([P[0, 2], P["x", "o"]], [1, 1]), dur=0.5)\n'))
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
_, slicedRequests = run_script(
    'p1 >> play("x-o", start=0.2, end=0.65, loop=False)', include_samples=True)
assert all(float(row[5]) == 0.2 and float(row[6]) == 0.65 and row[7] == '0'
           for row in slicedRequests)
_, patternSliceRequests = run_script(
    'p1 >> play("x-o-", start=P[0.0, 0.2], end=P[0.5, 0.9])',
    include_samples=True)
assert [float(row[5]) for row in patternSliceRequests] == [0.0, 0.2, 0.0, 0.2]
assert [float(row[6]) for row in patternSliceRequests] == [0.5, 0.9, 0.5, 0.9]
slicePattern256 = 'P[' + ','.join(['0.0'] * 256) + ']'
_, maxSliceRequests = run_script(
    'p1 >> play("x", start=' + slicePattern256 + ', end=1.0)',
    include_samples=True)
assert len(maxSliceRequests) == 256
_, expandedSliceRequests = run_script(
    'p1 >> play("x", start=P[0.0, 0.1, 0.2], end=0.8)',
    include_samples=True)
assert [float(row[5]) for row in expandedSliceRequests] == [0.0, 0.1, 0.2]
_, timevarSliceRequests = run_script(
    'p1 >> play("x-o-", start=var([0.0, 0.2], 2), '
    'end=var([0.6, 1.0], 2))', include_samples=True)
assert [float(row[5]) for row in timevarSliceRequests] == [0.0, 0.0, 0.2, 0.2]
assert [float(row[6]) for row in timevarSliceRequests] == [0.6, 0.6, 1.0, 1.0]
_, linearSliceRequests = run_script(
    'p1 >> play("x-o-", start=linvar([0.1, 0.3], 4), '
    'end=linvar([0.7, 0.9], 4))', include_samples=True)
assert [round(float(row[5]), 8) for row in linearSliceRequests] == [
    0.1, 0.15, 0.2, 0.25]
assert [round(float(row[6]), 8) for row in linearSliceRequests] == [
    0.7, 0.75, 0.8, 0.85]
_, loopRequests = run_script(
    'p1 >> play("x", dur=2, loop=True)', include_samples=True)
assert loopRequests[0][4:] == ['1', '0.0', '1.0', '1']
for bad in ('start=-0.1', 'end=1.1', 'start=0.5, end=0.5',
            'start=float("nan")', 'start=[]', 'start=P[0.0, 1.1]',
            'start=[0.0]*257',
            'start=P[0.2, 0.8], end=P[0.7, 0.6]',
            'start=var([0.8, 0.2], 2), end=var([0.6, 1.0], 2)',
            'loop=1'):
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
_, zippedSampleRequests, zippedSampleEvents = run_script(
    'p1 >> play(P["x", " "] & P["o", "-", " "], dur=0.5, sample=0)',
    include_samples=True, include_events=True)
assert [int(row[1]) for row in zippedSampleRequests] == [
    120, 111, 45, 120, 111, 120, 45], zippedSampleRequests
assert [float(row[1]) for row in zippedSampleEvents] == [
    0.0, 0.0, 0.5, 1.0, 1.5, 2.0, 2.0], zippedSampleEvents
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
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x"] & P["o"])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play((P["x", " "] & P["o", "-"]).rotate(1))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x"] & P[0])')
assert 'requires two finite P sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x"] & "o")')
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
numericChordZipText = (
    'p1 >> pluck(PZip(P[PGroup(0, 4, 7), None], '
    'P[PGroup(12, 16), PGroup(14, 19)]), dur=1, sus=0.7, oct=4)\n'
    'p2 >> triangle(PZip2(P[PGroup(0, 3), PGroup(2, 5)], '
    'P[PGroup(7, 11), PGroup(9, 12)], rule=lambda a, b: a != b), '
    'dur=1.5, sus=1.2, oct=3)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    numericChordZipText)
numericChordZipRows, numericChordZipEvents = run_script(
    numericChordZipText, include_events=True)
assert len(numericChordZipRows) == 2
assert len(numericChordZipEvents) == 15
assert all(int(event[3]) >= 0 for event in numericChordZipEvents)
assert sorted(float(event[1]) for event in numericChordZipEvents) == (
    [0.0] * 9 + [1.0] * 2 + [1.5] * 4)
assert list(namespace['PZip'](
    namespace['P'][namespace['PGroup'](0, 4, 7), None],
    namespace['P'][namespace['PGroup'](12, 16), namespace['PGroup'](14, 19)]
)) == [
    (namespace['PGroup'](0, 4, 7), namespace['PGroup'](12, 16)),
    (None, namespace['PGroup'](14, 19))]
assert list(namespace['PZip2'](
    namespace['P'][namespace['PGroup'](0, 4), namespace['PGroup'](2, 5)],
    namespace['P'][namespace['PGroup'](7, 11), namespace['PGroup'](9, 12)],
    rule=lambda a, b: a != b
)) == [
    (namespace['PGroup'](0, 4), namespace['PGroup'](7, 11)),
    (namespace['PGroup'](2, 5), namespace['PGroup'](9, 12))]
for chord_zip in (
        lambda: namespace['PZip'](
            namespace['P'][namespace['PGroup'](0, 1, 2, 3, 4),],
            namespace['P'][namespace['PGroup'](5, 6, 7, 8, 9),]),
        lambda: namespace['PZip2'](
            namespace['P'][namespace['PGroup'](0, 1, 2, 3, 4),],
            namespace['P'][namespace['PGroup'](5, 6, 7, 8, 9),])):
    try:
        chord_zip()
    except ValueError as error:
        assert 'limited to 8 notes' in str(error)
    else:
        raise AssertionError('PZip accepted a chord over the eight-note bound')
numericChordZipExample = run_example('140-numeric-pzip-chord-groups.py')
assert len(numericChordZipExample) == 2
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '140-numeric-pzip-chord-groups.py'),
        encoding='utf-8').read())
assert 'PZip requires 2 to 8 finite numeric or sample-symbol patterns' in run_compatibility_check(
    'p1 >> saw(PZip(P[0]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PZip(P["x", " "], P["o", "-"]))\n'
    'p2 >> play(PZip2(P["x", "-"], P["o", " "], rule=lambda a, b: a != b))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PZip(P["x", "o"], P[0, 1]))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PZip2(P["x"], P[0]))')
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
sampleZipText = (
    'p1 >> play(PZip(P["x", " "], P["o", "-", " "]), dur=1, sample=0)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(sampleZipText)
sampleZipRows, sampleZipRequests, sampleZipEvents = run_script(
    sampleZipText, include_samples=True, include_events=True)
assert len(sampleZipRows) == 1 and len(sampleZipRequests) == 7
assert sorted(int(row[1]) for row in sampleZipRequests) == [
    45, 45, 111, 111, 120, 120, 120]
assert len(sampleZipEvents) == 7
example92Text = open(
    os.path.join(sys.argv[2], '92-sample-pzip-pzip2.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example92Text)
example92Rows, example92Requests, example92Events = run_script(
    example92Text, include_samples=True, include_events=True)
assert len(example92Rows) == 2 and len(example92Requests) == 35
assert len(example92Events) == 35
example93Text = open(
    os.path.join(sys.argv[2], '93-clock-swing.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example93Text)
example93Rows = run_script(example93Text)
assert len(example93Rows) == 2
assert all(abs(float(row[22]) - 0.04) < 1e-9 for row in example93Rows)
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
example89Text = open(
    os.path.join(sys.argv[2], '89-nested-prhythm.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(example89Text)
example89Rows, example89Events = run_script(example89Text, include_events=True)
assert len(example89Rows) == 1 and len(example89Events) == 6
assert [round(float(event[1]) + float(event[7]), 6)
        for event in example89Events] == [
    0.0, 0.75, 1.0, 1.25, 2.0, 2.75]
assert [round(float(event[2]), 6) for event in example89Events] == [
    0.35] * 6
assert_player_polyphony_ok(example89Rows[0], example89Events)
groupedSustainText = (
    'p1 >> triangle(P[PGroup(0, 2, 4), PGroup(4, 6)], dur=1, '
    'sus=P[PGroup(0.25, 0.45), PGroup(0.35, 0.65)], oct=4, amp=0.14)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupedSustainText)
groupedSustainRows, groupedSustainEvents = run_script(
    groupedSustainText, include_events=True)
assert len(groupedSustainRows) == 1 and len(groupedSustainEvents) == 5
assert [round(float(event[1]), 6) for event in groupedSustainEvents] == [
    0.0, 0.0, 0.0, 1.0, 1.0]
assert [round(float(event[2]), 6) for event in groupedSustainEvents] == [
    0.25, 0.45, 0.25, 0.35, 0.65]
assert_player_polyphony_ok(groupedSustainRows[0], groupedSustainEvents)
groupedRhythmText = (
    'p1 >> triangle(P[PGroup(0, 2, 4), PGroup(0, 2, 4)], '
    'dur=PRhythm([1, (3, 8)]), sus=PGroup(0.2, 0.4, 0.6))\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupedRhythmText)
groupedRhythmRows, groupedRhythmEvents = run_script(
    groupedRhythmText, include_events=True)
assert len(groupedRhythmRows) == 1 and len(groupedRhythmEvents) == 6
assert [round(float(event[7]), 6) for event in groupedRhythmEvents] == [
    0.0, 0.0, 0.0, 0.0, 0.75, 1.5]
assert [round(float(event[2]), 6) for event in groupedRhythmEvents] == [
    0.2, 0.4, 0.6, 0.2, 0.4, 0.6]
sampleGroupedSustainRows, sampleGroupedSustainRequests, sampleGroupedSustainEvents = run_script(
    'p1 >> play("x o ", dur=1, sus=PGroup(0.25, 0.5), sample=0)\n',
    include_samples=True, include_events=True)
assert len(sampleGroupedSustainRows) == 1
assert len(sampleGroupedSustainRequests) == len(sampleGroupedSustainEvents) == 4
assert [int(row[1]) for row in sampleGroupedSustainRequests] == [
    120, 120, 111, 111]
assert [round(float(event[2]), 6) for event in sampleGroupedSustainEvents] == [
    0.25, 0.5, 0.25, 0.5]
badGroupedSustain = run_error_script(
    'p1 >> sine([0], dur=1, sus=PGroup(0, 0.5))\n')
assert 'each sus value must be finite and at least 0.02 beats' in badGroupedSustain
tooManyGroupedEvents = run_error_script(
    'p1 >> sine(P[0:33], dur=1, '
    'sus=PGroup(0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09))\n')
assert 'limited to 256 note events' in tooManyGroupedEvents
groupedSustainPolyphony = run_error_script(
    'p1 >> sine(P[0, 2], dur=1, '
    'sus=PGroup(1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5))\n')
assert 'pattern exceeds 8-note polyphony' in groupedSustainPolyphony
example90Text = open(
    os.path.join(sys.argv[2], '90-grouped-sustain.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example90Text)
example90Rows, example90Requests, example90Events = run_script(
    example90Text, include_samples=True, include_events=True)
assert len(example90Rows) == 2 and len(example90Requests) == 4
assert len(example90Events) == 9
assert [round(float(event[2]), 6) for event in example90Events[:5]] == [
    0.25, 0.45, 0.25, 0.35, 0.65]
assert [round(float(event[1]), 6) for event in example90Events[5:]] == [
    0.0, 0.0, 2.0, 2.0]
assert [round(float(event[2]), 6) for event in example90Events[5:]] == [
    0.25, 0.5, 0.25, 0.5]
assert_player_polyphony_ok(example90Rows[0], example90Events[:5])
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
playerFollowText = open(
    os.path.join(sys.argv[2], '109-player-degree-follow.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    playerFollowText)
followRows, followEvents = run_script(playerFollowText, include_events=True)
assert len(followRows) == 3 and len(followEvents) == 24
def player_event_pitches(rows, events, player_index):
    first = sum(int(row[5]) for row in rows[:player_index])
    count = int(rows[player_index][5])
    return [int(event[3]) for event in events[first:first + count]]
assert player_event_pitches(followRows, followEvents, 0) == player_event_pitches(
    followRows, followEvents, 1)
assert player_event_pitches(followRows, followEvents, 0) == player_event_pitches(
    followRows, followEvents, 2)
assert float(followRows[1][7]) == 0.32 and float(followRows[1][8]) == 0.2
assert float(followRows[2][7]) == 0.12 and float(followRows[2][8]) == 0.35
referenceEditRows, referenceEditEvents = run_script(
    'p2 >> bass([0, 4, 2, 5], dur=1, oct=3, amp=0.32, pan=0.2)\n'
    'p1 >> sine([0, 2, 4, 6], dur=0.5, amp=0.2)\n'
    'p2.follow(p1)\n'
    'p3 >> triangle([6, 2, 0, 4], dur=0.5, oct=5, amp=0.12, pan=0.35)\n'
    'p3.degree = p1.degree\n'
    'p1 >> pluck([1, 3, 5, 7], dur=0.5, oct=5, amp=0.24)\n',
    include_events=True)
assert len(referenceEditRows) == 3 and len(referenceEditEvents) == 12
sourcePitches = player_event_pitches(referenceEditRows, referenceEditEvents, 0)
assert player_event_pitches(referenceEditRows, referenceEditEvents, 1) == sourcePitches
assert player_event_pitches(referenceEditRows, referenceEditEvents, 2) == sourcePitches
assert float(referenceEditRows[1][7]) == 0.32
assert float(referenceEditRows[1][8]) == 0.2
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(2, "reverse")\n')
reverseRows, reverseEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(2, "reverse")\n',
    include_events=True)
assert len(reverseRows) == 1 and len(reverseEvents) == 8
assert float(reverseRows[0][4]) == 4.0
assert [int(event[3]) for event in reverseEvents] == [
    60, 62, 64, 65, 65, 64, 62, 60]
rotateRows, rotateEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(2, "rotate", 1)\n',
    include_events=True)
assert len(rotateRows) == 1 and len(rotateEvents) == 16
assert [int(event[3]) for event in rotateEvents[:12]] == [
    60, 62, 64, 65, 65, 60, 62, 64, 64, 65, 60, 62]
cycleReverseRows, cycleReverseEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(1, "reverse", cycle=4, ident="phrase")\n',
    include_events=True)
assert len(cycleReverseRows) == 1 and len(cycleReverseEvents) == 16
assert float(cycleReverseRows[0][4]) == 8.0
assert [int(event[3]) for event in cycleReverseEvents[:8]] == [
    60, 62, 62, 60, 65, 64, 62, 60]
identifiedStutterRows, identifiedStutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(0.5, "stutter", 2, cycle=2, ident="a")\n'
    'p1.every(1.5, "stutter", 2, cycle=2, ident="b")\n',
    include_events=True)
assert len(identifiedStutterRows) == 1
assert float(identifiedStutterRows[0][4]) == 2.0
assert len(identifiedStutterEvents) == 6
sameIdentRows, sameIdentEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(1, "reverse", cycle=4, ident="same")\n'
    'p1.every(3, "reverse", cycle=4, ident="same")\n',
    include_events=True)
lastIdentRows, lastIdentEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(3, "reverse", cycle=4, ident="same")\n',
    include_events=True)
assert sameIdentRows == lastIdentRows
assert sameIdentEvents == lastIdentEvents
cycleOftenRows, cycleOftenEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.often("shuffle", cycle=8, ident="variation")\n',
    include_events=True)
assert len(cycleOftenRows) == 1 and len(cycleOftenEvents) == 64
cycleSometimesRows, cycleSometimesEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.sometimes("shuffle", cycle=16, ident="variation")\n',
    include_events=True)
assert len(cycleSometimesEvents) == 128
cycleRarelyRows, cycleRarelyEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.rarely("shuffle", cycle=32, ident="variation")\n',
    include_events=True)
assert len(cycleRarelyEvents) == 256
cycleIdentText = open(
    os.path.join(sys.argv[2], '114-cycle-ident-schedules.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    cycleIdentText)
cycleIdentRows, cycleIdentEvents = run_script(
    cycleIdentText, include_events=True)
assert len(cycleIdentRows) == 1 and 4 <= len(cycleIdentEvents) <= 256
assert_player_polyphony_ok(cycleIdentRows[0], cycleIdentEvents)
for scheduleSeed in range(16):
    seededCycleRows, seededCycleEvents = run_script_with_schedule_seed(
        cycleIdentText, scheduleSeed)
    assert len(seededCycleRows) == 1
    assert 4 <= len(seededCycleEvents) <= 256
    assert_player_polyphony_ok(seededCycleRows[0], seededCycleEvents)
sometimesText = open(
    os.path.join(sys.argv[2], '110-every-sometimes-players.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sometimesText)
sometimesRows, sometimesEvents = run_script(
    sometimesText, include_events=True)
assert len(sometimesRows) == 2
assert 2 < len(sometimesEvents) <= 256
assert int(sometimesRows[1][5]) <= 256
assert 8.0 <= float(sometimesRows[1][4]) <= 128.0
shuffleRows, shuffleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(1, "shuffle")\n',
    include_events=True)
assert len(shuffleRows) == 1 and len(shuffleEvents) == 4
assert set(int(event[3]) for event in shuffleEvents) <= {60, 62, 64, 65}
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.often("shuffle")\n')
oftenRows, oftenEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.often("shuffle")\n',
    include_events=True)
assert len(oftenRows) == 1 and 4 <= len(oftenEvents) <= 256
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.rarely("reverse")\n')
rarelyRows, rarelyEvents = run_script(
    'p1 >> sine([0, 1], dur=0.25)\np1.rarely("reverse")\n',
    include_events=True)
assert len(rarelyRows) == 1 and 4 <= len(rarelyEvents) <= 256
stutterRows, stutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter", 3)\n',
    include_events=True)
assert len(stutterRows) == 1 and len(stutterEvents) == 8
assert sorted(int(event[3]) for event in stutterEvents) == [
    60, 60, 60, 62, 64, 64, 64, 65]
assert_player_polyphony_ok(stutterRows[0], stutterEvents)
defaultStutterRows, defaultStutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter")\n',
    include_events=True)
assert len(defaultStutterRows) == 1 and len(defaultStutterEvents) == 6
stackedRows, stackedEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter", 3)\np1.every(2, "reverse")\n',
    include_events=True)
assert len(stackedRows) == 1 and len(stackedEvents) <= 256
assert_player_polyphony_ok(stackedRows[0], stackedEvents)
singleUpdatedRows, singleUpdatedEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(1, "reverse")\n',
    include_events=True)
replacedScheduleRows, replacedScheduleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(2, "reverse")\np1.every(1, "reverse")\n',
    include_events=True)
assert replacedScheduleRows == singleUpdatedRows
assert replacedScheduleEvents == singleUpdatedEvents
sampleStutterRows, sampleStutterRequests, sampleStutterEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0, amp=0.3)\n'
    'p1.every(1, "stutter", 3)\n',
    include_samples=True, include_events=True)
assert len(sampleStutterRows) == 1
assert [int(row[1]) for row in sampleStutterRequests] == [
    120, 120, 120, 45, 111, 111, 111, 45], (
        sampleStutterRequests, sampleStutterEvents)
assert [round(float(row[1]), 6) for row in sampleStutterEvents] == [
    0.0, 0.166667, 0.333333, 0.5, 1.0, 1.166667, 1.333333, 1.5]
_, restStutterRequests, restStutterEvents = run_script(
    'p1 >> play("x o ", dur=0.5, sample=0)\n'
    'p1.every(0.5, "stutter", 3)\n',
    include_samples=True, include_events=True)
assert len(restStutterRequests) == 6
assert len(restStutterEvents) == 6
_, identifiedSampleRequests, identifiedSampleEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'p1.every(0.5, "stutter", 2, cycle=2, ident="first")\n'
    'p1.every(1.5, "stutter", 2, cycle=2, ident="second")\n',
    include_samples=True, include_events=True)
assert len(identifiedSampleRequests) == len(identifiedSampleEvents) == 6
assert 'Sample players currently support scheduled stutter only' in run_error_script(
    'p1 >> play("x-o-", dur=0.5)\np1.every(1, "shuffle")\n')
assert 'unstrummed hits without per-note delay' in run_error_script(
    'p1 >> play("x-o-", dur=0.5, strum=0.05)\n'
    'p1.every(1, "stutter", 3)\n')
scheduleShortcutText = open(
    os.path.join(sys.argv[2], '111-player-schedule-shortcuts.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    scheduleShortcutText)
scheduleRows, scheduleEvents = run_script(
    scheduleShortcutText, include_events=True)
assert len(scheduleRows) == 3 and len(scheduleEvents) <= 256
assert int(scheduleRows[2][5]) > 4
assert_player_polyphony_ok(scheduleRows[2], scheduleEvents[
    int(scheduleRows[0][5]) + int(scheduleRows[1][5]):])
for scheduleSeed in range(16):
    seededRows, seededEvents = run_script_with_schedule_seed(
        scheduleShortcutText, scheduleSeed)
    assert len(seededRows) == 3 and len(seededEvents) <= 256
    assert all(int(row[5]) <= 256 for row in seededRows)
drumStutterText = open(
    os.path.join(sys.argv[2], '112-drum-sample-stutter.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    drumStutterText)
drumStutterRows, drumStutterRequests, drumStutterEvents = run_script(
    drumStutterText, include_samples=True, include_events=True)
assert len(drumStutterRows) == 8 and len(drumStutterRequests) == 8, (
    drumStutterRows, drumStutterRequests, drumStutterEvents)
assert drumStutterRows[0][2] == '0' and drumStutterRows[0][4] == '2.0'
assert drumStutterRows[0][5] == '8'
assert [int(row[1]) for row in drumStutterRequests].count(120) == 3
assert [int(row[1]) for row in drumStutterRequests].count(111) == 3
assert len(drumStutterEvents) == 8
stackedScheduleText = open(
    os.path.join(sys.argv[2], '113-stacked-player-schedules.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    stackedScheduleText)
stackedScheduleRows, stackedScheduleEvents = run_script(
    stackedScheduleText, include_events=True)
assert len(stackedScheduleRows) == 1
assert 4 <= len(stackedScheduleEvents) <= 256
assert_player_polyphony_ok(stackedScheduleRows[0], stackedScheduleEvents)
for scheduleSeed in range(16):
    seededDrumRows, seededDrumRequests, seededDrumEvents = (
        run_script_with_schedule_seed(
            drumStutterText, scheduleSeed, include_samples=True))
    assert len(seededDrumRequests) == len(seededDrumEvents) <= 256
for scheduleSeed in range(16):
    seededStackRows, seededStackEvents = run_script_with_schedule_seed(
        stackedScheduleText, scheduleSeed)
    assert len(seededStackRows) == 1
    assert 4 <= len(seededStackEvents) <= 256
sampleOftenStutter = (
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'p1.often("stutter", 3)\n')
for scheduleSeed in range(16):
    seededOftenRows, seededOftenRequests, seededOftenEvents = (
        run_script_with_schedule_seed(
            sampleOftenStutter, scheduleSeed, include_samples=True))
    assert len(seededOftenRequests) == len(seededOftenEvents) <= 256
sampleStutterOverflow = run_error_script(
    'p1 >> play("xxxxxxxxxxxxxxxxxxxx", dur=0.05)\n'
    'p1.every(0.05, "stutter", 16)\n')
assert 'scheduled stutter expands beyond the 256-event limit' in (
    sampleStutterOverflow.lower()), sampleStutterOverflow
assert 'Scheduled player operations currently need a dense' in run_error_script(
    'p1 >> sine([0, None, 2, None], dur=0.5)\n'
    'p1.every(2, "reverse")\n')
assert 'Scheduled transforms and degree-follow relationships cannot be combined' in run_error_script(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\n'
    'p2 >> bass([0, 2, 4, 6], dur=0.5)\n'
    'p2.follow(p1)\np2.every(2, "reverse")\n')
sampleUnsupportedStack = run_error_script(
    'p1 >> play("x-o-", dur=0.5)\n'
    'p1.every(1, "stutter", 2)\np1.every(2, "shuffle")\n')
assert 'Sample players currently support scheduled stutter only' in (
    sampleUnsupportedStack), sampleUnsupportedStack
assert 'cannot follow itself' in run_compatibility_check(
    'p1 >> sine([0, 2], dur=0.5)\np1.follow(p1)\n')
assert 'degree assignment supports only' in run_compatibility_check(
    'p1 >> sine([0, 2], dur=0.5)\np1.degree = 3\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(2, "stutter", 17)\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\n'
    'p1.every(2, "reverse", cycle=var([8, 16], 4))\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(var([2, 4], 2), "reverse")\n')
dense_degrees = ', '.join(str(index % 4) for index in range(100))
assert 'beyond the 256-event limit' in run_error_script(
    'p1 >> sine([' + dense_degrees + '], dur=0.1)\n'
    'p1.every(6, "reverse")\n')
assert 'Circular player degree reference' in run_error_script(
    'p1 >> sine([0, 2], dur=0.5)\np2 >> bass([2, 4], dur=0.5)\n'
    'p1.follow(p2)\np2.follow(p1)\n')

groupExampleText = open(
    os.path.join(sys.argv[2], '115-player-groups.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupExampleText)
groupRows, groupEvents = run_script(
    groupExampleText, include_events=True)
assert len(groupRows) == 3 and len(groupEvents) <= 256
assert all(abs(float(groupRows[index][7]) - 0.22) < 1e-9
           for index in (0, 1))
assert abs(float(groupRows[0][9]) - 800.0) < 1e-9
assert abs(float(groupRows[1][9]) - 800.0) < 1e-9
assert groupRows[2][7] == '0.4'

groupPitchTimingText = open(
    os.path.join(sys.argv[2], '116-group-pitch-and-timing.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupPitchTimingText)
exampleGroupRows, exampleGroupBuffers, exampleGroupEvents = run_script(
    groupPitchTimingText, include_samples=True, include_events=True)
assert len(exampleGroupRows) == 3 and len(exampleGroupBuffers) == 4
assert len(exampleGroupEvents) == 12
groupDegreeTimevarText = open(
    os.path.join(sys.argv[2], '117-group-degree-timevars.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupDegreeTimevarText)
degreeTimevarExampleRows, degreeTimevarExampleEvents = run_script(
    groupDegreeTimevarText, include_events=True)
assert len(degreeTimevarExampleRows) == 3
assert len(degreeTimevarExampleEvents) == 12
groupRootText = open(
    os.path.join(sys.argv[2], '118-group-root-control.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupRootText)
groupRootExampleRows, groupRootExampleEvents = run_script(
    groupRootText, include_events=True)
assert len(groupRootExampleRows) == 3
assert len(groupRootExampleEvents) == 12
globalRootText = open(
    os.path.join(sys.argv[2], '119-global-root-timevars.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    globalRootText)
globalRootRows, globalRootEvents = run_script(
    globalRootText, include_events=True)
assert len(globalRootRows) == 4 and len(globalRootEvents) == 16
patternHelperText = open(
    os.path.join(sys.argv[2], '120-pattern-shaping-helpers.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternHelperText)
patternHelperRows, patternHelperEvents = run_script(
    patternHelperText, include_events=True)
assert len(patternHelperRows) == 3
assert 0 < len(patternHelperEvents) <= 256
patternMethodText = open(
    os.path.join(sys.argv[2], '121-pattern-method-parity.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternMethodText)
patternMethodRows, patternMethodEvents = run_script(
    patternMethodText, include_events=True)
assert len(patternMethodRows) == 3
assert 0 < len(patternMethodEvents) <= 256
pitchTransformText = open(
    os.path.join(sys.argv[2], '122-pitch-inversion.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    pitchTransformText)
pitchTransformRows, pitchTransformEvents = run_script(
    pitchTransformText, include_events=True)
assert len(pitchTransformRows) == 3
assert 0 < len(pitchTransformEvents) <= 256
patternCompositionText = open(
    os.path.join(sys.argv[2], '123-arpeggio-and-splice.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternCompositionText)
patternCompositionRows, patternCompositionEvents = run_script(
    patternCompositionText, include_events=True)
assert len(patternCompositionRows) == 3
assert 0 < len(patternCompositionEvents) <= 256
sortUnduplicateText = open(
    os.path.join(sys.argv[2], '124-sort-and-unduplicate.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sortUnduplicateText)
sortUnduplicateRows, sortUnduplicateEvents = run_script(
    sortUnduplicateText, include_events=True)
assert len(sortUnduplicateRows) == 3
assert 0 < len(sortUnduplicateEvents) <= 256
patternReplaceText = open(
    os.path.join(sys.argv[2], '125-pattern-replace.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternReplaceText)
patternReplaceRows, patternReplaceEvents = run_script(
    patternReplaceText, include_events=True)
assert len(patternReplaceRows) == 3
assert 0 < len(patternReplaceEvents) <= 256
patternLayerText = open(
    os.path.join(sys.argv[2], '126-pattern-layer.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternLayerText)
patternLayerRows, patternLayerEvents = run_script(
    patternLayerText, include_events=True)
assert len(patternLayerRows) == 3
assert 0 < len(patternLayerEvents) <= 256
patternTrimText = open(
    os.path.join(sys.argv[2], '127-pattern-trim.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternTrimText)
patternTrimRows, patternTrimEvents = run_script(
    patternTrimText, include_events=True)
assert len(patternTrimRows) == 3
assert 0 < len(patternTrimEvents) <= 256
patternRandomText = open(
    os.path.join(sys.argv[2], '128-normalise-and-choose.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternRandomText)
patternRandomRows, patternRandomEvents = run_script(
    patternRandomText, include_events=True)
assert len(patternRandomRows) == 3
assert 0 < len(patternRandomEvents) <= 256
patternZipText = open(
    os.path.join(sys.argv[2], '129-pattern-zip-methods.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternZipText)
patternZipRows, patternZipEvents = run_script(
    patternZipText, include_events=True)
assert len(patternZipRows) == 3
assert 0 < len(patternZipEvents) <= 256
patternMapPivotText = open(
    os.path.join(sys.argv[2], '130-pattern-map-pivot.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternMapPivotText)
patternMapPivotRows, patternMapPivotEvents = run_script(
    patternMapPivotText, include_events=True)
assert len(patternMapPivotRows) == 3
assert 0 < len(patternMapPivotEvents) <= 256
patternShuffleText = open(
    os.path.join(sys.argv[2], '131-true-shuffle-and-shufflets.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternShuffleText)
patternShuffleRows, patternShuffleEvents = run_script(
    patternShuffleText, include_events=True)
assert len(patternShuffleRows) == 3
assert 0 < len(patternShuffleEvents) <= 256
patternWalkText = open(
    os.path.join(sys.argv[2], '132-pwalk-melodic-walk.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternWalkText)
patternWalkRows, patternWalkEvents = run_script(
    patternWalkText, include_events=True)
assert len(patternWalkRows) == 3
assert 0 < len(patternWalkEvents) <= 256
patternChainText = open(
    os.path.join(sys.argv[2], '133-pchain-transition-melody.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternChainText)
patternChainRows, patternChainEvents = run_script(
    patternChainText, include_events=True)
assert len(patternChainRows) == 3
assert 0 < len(patternChainEvents) <= 256
patternDeltaText = open(
    os.path.join(sys.argv[2], '134-pdelta-pitch-contour.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternDeltaText)
patternDeltaRows, patternDeltaEvents = run_script(
    patternDeltaText, include_events=True)
assert len(patternDeltaRows) == 3
assert 0 < len(patternDeltaEvents) <= 256
patternSquareText = open(
    os.path.join(sys.argv[2], '135-psquare-pitch-sequence.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternSquareText)
patternSquareRows, patternSquareEvents = run_script(
    patternSquareText, include_events=True)
assert len(patternSquareRows) == 3
assert 0 < len(patternSquareEvents) <= 256
groupPitchRows, groupPitchEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([1, 3], dur=1, oct=4)\n'
    'p2 >> pluck([2, 5], dur=1, oct=5)\n'
    'pair = Group(p1, p2)\n'
    'pair.degree = P[0, 2, 4, 6]\n'
    'pair.scale = Scale.minor\n'
    'pair.oct = var([3, 4], [1, 1])\n'
    'pair.dur = var([0.5, 1], [1, 1])\n'
    'pair.sus = 0.35\n', include_events=True)
assert len(groupPitchRows) == 2 and len(groupPitchEvents) == 8
expectedGroupPitches = [36, 39, 55, 46]
for playerEvents in (groupPitchEvents[:4], groupPitchEvents[4:]):
    assert [int(event[3]) for event in playerEvents] == expectedGroupPitches
    assert [round(float(event[1]), 6) for event in playerEvents] == [
        0.0, 0.5, 1.0, 2.0]
    assert all(abs(float(event[2]) - 0.35) < 1e-9
               for event in playerEvents)

groupBeforeRun = run_script(
    'pair = Group(p1, p2)\n'
    'pair.degree = P[1, 3]\n'
    'pair.scale = Scale.dorian\n'
    'pair.oct = 4\npair.dur = 0.5\npair.sus = 0.25\n'
    'p1 >> triangle([0, 2, 4], oct=5)\n'
    'p2 >> saw([3, 5], oct=5)\n')
assert len(groupBeforeRun) == 2
assert all(int(row[5]) == 2 for row in groupBeforeRun)
assert all(abs(float(row[4]) - 1.0) < 1e-9 for row in groupBeforeRun)

groupPitchReassignmentRows, groupPitchReassignmentEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([5, 6], oct=4, scale=Scale.major)\n'
    'pair = Group(p1)\n'
    'pair.degree = P[0, 1]\n'
    'pair.scale = Scale.majorPentatonic\n'
    'pair.oct = 3\n'
    'pair.scale = Scale.minor\n'
    'pair.oct = 5\n',
    include_events=True)
assert len(groupPitchReassignmentRows) == 1
assert [int(event[3]) for event in groupPitchReassignmentEvents] == [60, 62]

groupRootRows, groupRootEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, 2, 4, 6], dur=1, oct=5)\n'
    'p2 >> pluck([0, 2, 4, 6], dur=1, oct=4)\n'
    'pair = Group(p1, p2)\n'
    'pair.root = "D"\n',
    include_events=True)
assert len(groupRootRows) == 2 and len(groupRootEvents) == 8
assert [int(event[3]) for event in groupRootEvents[:4]] == [62, 66, 69, 73]
assert [int(event[3]) for event in groupRootEvents[4:]] == [50, 54, 57, 61]
assert 'No unsupported constructs detected' in run_compatibility_check(
    'pair = Group(p1, p2)\npair.root = "D"\n')

preassignedRootRows, preassignedRootEvents = run_script(
    'Scale.default.set("major")\n'
    'pair = Group(p1)\npair.root = 7\n'
    'p1 >> triangle([0, 2], dur=0.5, oct=5)\n',
    include_events=True)
assert len(preassignedRootRows) == 1
assert [int(event[3]) for event in preassignedRootEvents] == [67, 71]

def group_root_events(root_assignment):
    return run_script(
        'Scale.default.set("major")\nRoot.default.set("C")\n'
        'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=5)\n'
        'pair = Group(p1)\npair.root = ' + root_assignment + '\n',
        include_events=True)[1]

steppedGroupRootEvents = group_root_events('var([0, 7], [1, 1])')
assert [int(event[3]) for event in steppedGroupRootEvents] == [
    60, 60, 67, 67]
linearGroupRootEvents = group_root_events('linvar([0, 4], [1, 1])')
assert [int(event[3]) for event in linearGroupRootEvents] == [
    60, 62, 64, 62]

playerRootEvents = run_script(
    'Scale.default.set("major")\n'
    'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=5, '
    'root=var([0, 7], [1, 1]))\n',
    include_events=True)[1]
assert [int(event[3]) for event in playerRootEvents] == [60, 60, 67, 67]
assert 'root var/linvar must use pitch classes 0..11 or MIDI notes 12..127' in (
    run_error_script(
        'p1 >> triangle([0, 0], root=var([0, 12], [1, 1]))\n'))
assert 'root must be a pitch class' in run_error_script(
    'pair = Group(p1)\npair.root = "H"\n')

def group_degree_events(degree_assignment):
    return run_script(
        'Scale.default.set("major")\nRoot.default.set("C")\n'
        'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=4)\n'
        'p2 >> pluck([0, 0, 0, 0], dur=0.5, oct=5)\n'
        'pair = Group(p1, p2)\n'
        'pair.degree = ' + degree_assignment + '\n',
        include_events=True)[1]

dynamicGroupDegreeEvents = group_degree_events(
    'var([0, 2, 4, 6], [0.5, 0.5, 0.5, 0.5])')
staticGroupDegreeEvents = group_degree_events('P[0, 2, 4, 6]')
assert len(dynamicGroupDegreeEvents) == 8
assert [int(event[3]) for event in dynamicGroupDegreeEvents] == [
    int(event[3]) for event in staticGroupDegreeEvents]
linearGroupDegreeEvents = group_degree_events('linvar([0, 4], [1, 1])')
steppedLinearDegreeEvents = group_degree_events('P[0, 2, 4, 2]')
assert [int(event[3]) for event in linearGroupDegreeEvents] == [
    int(event[3]) for event in steppedLinearDegreeEvents]

preassignedGroupRows, preassignedGroupEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'pair = Group(p1, p2)\n'
    'pair.degree = var([0, 2], [0.5, 0.5])\n'
    'p1 >> triangle([0, 0], dur=0.5, oct=4)\n'
    'p2 >> pluck([0, 0], dur=0.5, oct=5)\n',
    include_events=True)
preassignedStaticRows, preassignedStaticEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, 0], dur=0.5, oct=4)\n'
    'p2 >> pluck([0, 0], dur=0.5, oct=5)\n'
    'Group(p1, p2).degree = P[0, 2]\n',
    include_events=True)
assert len(preassignedGroupRows) == 2 and len(preassignedGroupEvents) == 4
assert [int(event[3]) for event in preassignedGroupEvents] == [
    int(event[3]) for event in preassignedStaticEvents]

chordDegreeRows, chordDegreeEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([[0, 2, 4], [0, 2, 4]], dur=1, oct=5)\n'
    'Group(p1).degree = var([0, 2], [1, 1])\n',
    include_events=True)
assert len(chordDegreeRows) == 1 and len(chordDegreeEvents) == 6
assert [int(event[3]) for event in chordDegreeEvents[:3]] == [60, 64, 67]
assert [int(event[3]) for event in chordDegreeEvents[3:]] == [64, 67, 71]

restingGroupDegreeRows, restingGroupDegreeEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, None, 0, 0], dur=0.5, oct=5)\n'
    'Group(p1).degree = var([0, 2, 4, 6], [0.5, 0.5, 0.5, 0.5])\n',
    include_events=True)
assert len(restingGroupDegreeRows) == 1
assert [float(event[1]) for event in restingGroupDegreeEvents] == [0.0, 1.0, 1.5]

sampleGroupRows, sampleGroupBuffers, sampleGroupEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'drums = Group(p1)\ndrums.dur = 1\ndrums.sus = 0.25\n',
    include_samples=True, include_events=True)
assert len(sampleGroupRows) == 1 and len(sampleGroupBuffers) == 4
assert len(sampleGroupEvents) == 4
assert 'numeric note players, not sample players' in run_error_script(
    'p1 >> play("x-o-", sample=0)\nGroup(p1).degree = P[0, 2]\n')
for invalidGroupAssignment in (
        'Group(p1).dur = 0\n',
        'Group(p1).sus = 0.01\n',
        'Group(p1).oct = 11\n',
        'Group(p1).degree = P["x", "o"]\n',
        'Group(p1).degree = var([0, 128], 1)\n'):
    assert run_error_script(invalidGroupAssignment)

groupSetAfterRun = run_script(
    'p1 >> sine([0, 2, 4, 6], amp=0.8)\n'
    'p2 >> triangle([1, 3, 5, 7], amp=0.7)\n'
    'pair = Group(p1, p2)\n'
    'pair.amp = 0.3\n'
    'pair.pan = -0.25\n'
    'pair.reverb = "none"\n')
assert len(groupSetAfterRun) == 2
assert all(abs(float(row[7]) - 0.3) < 1e-9 for row in groupSetAfterRun)
assert all(abs(float(row[8]) + 0.25) < 1e-9 for row in groupSetAfterRun)
assert all(abs(float(row[9]) - 20000.0) < 1e-9 for row in groupSetAfterRun)

groupSetBeforeRun = run_script(
    'pair = Group(p1, p2)\n'
    'pair.cutoff = 1250\n'
    'p1 >> sine([0, 2], cutoff=700)\n'
    'p2 >> bass([0, 3], cutoff=900)\n')
assert len(groupSetBeforeRun) == 2
assert all(abs(float(row[9]) - 1250.0) < 1e-9
           for row in groupSetBeforeRun)

allGroupControls = (
    'pair = Group(p1, p2)\n'
    'pair.amp = 0.3\npair.pan = -0.2\npair.cutoff = 1500\n'
    'pair.echo = 0.25\npair.delaytime = 0.2\n'
    'pair.atk = 0.02\npair.rel = 0.18\npair.reverb = "gverb"\n'
    'pair.room = 0.6\npair.mix = 0.25\npair.damp = 0.4\n'
    'pair.revtime = 2.5\npair.spread = 30\n'
    'pair.lpf = 1200\npair.lpr = 0.75\npair.hpf = 60\n'
    'pair.hpr = 0.8\npair.bpf = 700\npair.bpr = 0.65\n'
    'pair.decay = 2\npair.echomix = 0.4\npair.feedback = 0.5\n'
    'pair.bpnoise = 0.2\npair.swell = 0.3\n'
    'p1 >> sine([0, 2], dur=1)\n'
    'p2 >> triangle([1, 3], dur=1)\n')
allGroupRows, allGroupEffects, allGroupFilters, allGroupDecay, allGroupMod = (
    run_script(allGroupControls, include_group_controls=True))
assert len(allGroupRows) == 2
assert all(abs(float(row[7]) - 0.3) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[8]) + 0.2) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[9]) - 1500.0) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[10]) - 0.25) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[11]) - 0.2) < 1e-9 for row in allGroupRows)
assert len(allGroupEffects) == 2 and all(row[2] == '3' for row in allGroupEffects)
assert all([float(value) for value in row[3:8]] ==
           [0.25, 0.6, 0.4, 2.5, 30.0] for row in allGroupEffects)
assert len(allGroupFilters) == 2
assert all([float(value) for value in row[2:8]] ==
           [1200.0, 0.75, 60.0, 0.8, 700.0, 0.65]
           for row in allGroupFilters)
assert len(allGroupDecay) == 2
assert all([float(row[index]) for index in (2, 4, 6)] ==
           [2.0, 0.4, 0.5] for row in allGroupDecay)
assert len(allGroupMod) == 2
assert all([float(value) for value in row[2:4]] == [0.2, 0.3]
           for row in allGroupMod)

groupScheduleRows, groupScheduleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p2 >> triangle([3, 2, 1, 0], dur=0.5)\n'
    'pair = Group(p1, p2)\n'
    'pair.every(1, "reverse", cycle=4, ident="shared")\n',
    include_events=True)
assert len(groupScheduleRows) == 2 and len(groupScheduleEvents) == 32
assert [event[1] for event in groupScheduleEvents[:16]] == [
    event[1] for event in groupScheduleEvents[16:]]

groupStopRows = run_script(
    'p1 >> sine([0, 2])\np2 >> triangle([1, 3])\np3 >> bass([0, 4])\n'
    'Group(p1, p2).stop()\n')
assert [row[2] for row in groupStopRows] == ['1', '1', '0']

groupOnlyRows = run_script(
    'p1 >> sine([0, 2])\np2 >> triangle([1, 3])\np3 >> bass([0, 4])\n'
    'Group(p1, p2).only()\n')
assert [row[2] for row in groupOnlyRows] == [
    '0', '0', '1']

groupSoloRows, groupSoloMutes = run_script(
    'p1 >> sine([0])\np2 >> triangle([0])\np3 >> bass([0])\n'
    'Group(p1, p2).solo()\n', include_mutes=True)
assert len(groupSoloRows) == 3 and len(groupSoloMutes) == 8
assert [int(row[2]) for row in groupSoloMutes] == [
    0, 0, 1, 1, 1, 1, 1, 1]

masterRows = run_script(
    'p1 >> sine([0], amp=0.8)\np2 >> triangle([0], amp=0.7)\n'
    'p2.stop()\nMaster().amp = 0.25\n')
assert len(masterRows) == 2
assert abs(float(masterRows[0][7]) - 0.25) < 1e-9
assert masterRows[1][2] == '1'

assert 'Group accepts zero to eight distinct players' in run_compatibility_check(
    'Group(p1, p1)\n')
assert 'Unsupported Group attribute' in run_compatibility_check(
    'pair = Group(p1, p2)\npair.not_a_control = 1\n')
assert 'Group.add accepts one player name' in run_compatibility_check(
    'pair = Group(p1)\npair.add(p9)\n')

sample_directory = os.path.join(sys.argv[2], 'Samples')
assert len([name for path, dirs, files in os.walk(sample_directory) for name in files if name.endswith('.wav')]) == 6
print('Smoke tests passed: embedded pattern/player API, Pvar phrase switching, Clock.bpm var/linvar, compatibility checks, diagnostics, WAV samples, user SynthDefs, and Portable examples.')
