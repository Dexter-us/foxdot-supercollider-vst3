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
processor_source = open(
    os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])),
                 'processor.cpp'),
    encoding='utf-8', errors='replace').read()
for wow_fragment in (
        'bool smoothNoise = false;',
        'double noisePeriod = 0.0;',
        "token[0] != 'w'",
        "const bool smoothNoise = token[0] == 'w';",
        'result.smoothNoise = smoothNoise;',
        'if (control.smoothNoise)',
        '951.135664',
        'bnoise(position * 2.3)'):
    if wow_fragment not in processor_source:
        raise SystemExit('Native smooth-noise wow control is missing: ' +
                         wow_fragment)

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
    return ''.join(parts)

safe_names = ('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
              'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
              'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str',
              'sum', 'tuple', 'zip', 'ValueError', 'TypeError',
              'ZeroDivisionError', '__build_class__')
example_synth_names = (
    'CustomPluck', 'CustomTone', 'AirwindowsTapeDelay2', 'AirwindowsEchojam',
    'JPverb', 'Greyhole', 'Chorus', 'Flanger', 'Phaser', 'Tremolo',
    'LimutPingPong', 'LimutDuck', 'LimutDisperser', 'LimutFuzz',
    'LimutShifter', 'LimutShimmer', 'LimutGatedVerb', 'LimutCompressor',
    'JazzKitKick', 'JazzKitBrushKick', 'JazzKitBrushTap', 'JazzKitBrushSweep',
    'JazzKitClosedHat', 'JazzKitOpenHat', 'JazzKitRide', 'JazzKitCrossStick',
    'JazzKitRackTom', 'JazzKitFloorTom',
    'SCL_bass_acidOto3091', 'SCL_bass_bassWarsaw', 'SCL_bass_moogBass',
    'SCL_bass_subBass1', 'SCL_bells_glockenspiel', 'SCL_bells_prayerBell',
    'SCL_drums_hihat1', 'SCL_drums_kick808', 'SCL_drums_snare909',
    'SCL_guitar_modalElectricGuitar', 'SCL_guitar_pluck',
    'SCL_keyboards_FMRhodes1', 'SCL_keyboards_harpsichord1',
    'SCL_organ_organTonewheel0', 'SCL_pads_sawSynth',
    'SCL_percussion_kalimba', 'SCL_percussion_marimba1',
    'SCL_strings_violin', 'SCLOrkPhazerFX', 'SCLOrkDecimatorFX',
    'SCLOrkCrossoverDistortionFX')
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_root_control': None,
    '_dexter_updates': {}, '_dexter_mutes': {},
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
oneShot = namespace['riser'](8, 0.1, 0.9)
assert oneShot.wire() == 'r:0.1,0.9:8.0'
assert oneShot.at(-1) == 0.1 and oneShot.at(0) == 0.1
assert abs(oneShot.at(4) - 0.5) < 1e-9
assert oneShot.at(8) == 0.9 and oneShot.at(1000) == 0.9
dropPattern = namespace['drop'](30, 2)
assert dropPattern.wire() == 'v:1.0,0.0:30.0,2.0'
assert dropPattern.at(0) == 1.0 and dropPattern.at(29.99) == 1.0
assert dropPattern.at(30) == 0.0 and dropPattern.at(31.99) == 0.0
assert dropPattern.at(32) == 1.0 and dropPattern.at(64) == 1.0
customDrop = namespace['drop'](8, 1, high=8000, low=400)
assert customDrop.wire() == 'v:8000.0,400.0:8.0,1.0'
wowDefault = namespace['wow']()
wowSameSeed = namespace['wow'](seed=0)
wowOtherSeed = namespace['wow'](seed=1)
assert len(wowDefault.values) == 2 and wowDefault.values == [-0.3, 0.3]
assert wowDefault.wire() == 'w:-0.3,0.3:2.0:0'
assert wowDefault.wire() == wowSameSeed.wire()
assert wowDefault.wire() != wowOtherSeed.wire()
assert abs(wowDefault.at(0.0) - (-0.1)) < 1e-12
assert all(-0.3 <= wowDefault.at(beat) <= 0.3
           for beat in (0.0, 0.125, 0.5, 1.0, 2.0, 12.0))
assert wowDefault.at(0.75) == wowSameSeed.at(0.75)
assert wowDefault.at(0.75) != wowOtherSeed.at(0.75)
assert namespace['wow'](amount=0.2, center=0.5).values == [0.3, 0.7]
tgDefault = namespace['tg']()
assert tgDefault.wire() == 'v:1.0,0.0:0.125,0.125'
assert tgDefault.at(0) == 1.0 and tgDefault.at(0.124) == 1.0
assert tgDefault.at(0.125) == 0.0 and tgDefault.at(0.25) == 1.0
assert namespace['tg'](0.5, high=0.8, low=0.1).wire() == 'v:0.8,0.1:0.5,0.5'
for badWow in ((-0.1, 2), (0.3, 0), (0.3, 0.1), (0.3, 4097),
               (True, 2), (float('inf'), 2), (0.3, 2, True)):
    try:
        if len(badWow) == 2:
            namespace['wow'](badWow[0], badWow[1])
        elif len(badWow) == 3:
            namespace['wow'](badWow[0], badWow[1], badWow[2])
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('wow must reject invalid or unbounded arguments')
for badTg in (0, -1, 4097, True, float('nan')):
    try:
        namespace['tg'](badTg)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('tg must reject invalid or unbounded step durations')
for badSeed in (True, 1.5, 2147483648):
    try:
        namespace['wow'](seed=badSeed)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('wow must reject invalid seeds')
for badDuration in (0, -1, 4096.001, True, float('inf')):
    try:
        namespace['riser'](badDuration)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('riser must reject invalid or unbounded durations')
try:
    namespace['riser'](8, 0, float('inf'))
except (TypeError, ValueError):
    pass
else:
    raise AssertionError('riser must reject non-finite endpoints')
for badDrop in ((0, 2), (30, 0), (4097, 2), (30, float('inf')),
                (True, 2), (30, 2, float('nan'), 0)):
    try:
        namespace['drop'](*badDrop)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('drop must reject invalid durations and levels')
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

def run_script(script_text, include_events=False, include_effects=False, include_filters=False, include_decay=False, include_mutes=False, include_modulations=False, include_effect_modulations=False, include_gates=False, user_synth_names=()):
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
        assert lines[0].startswith('OK\t') and 0 <= int(lines[0].split('\t')[1]) <= 8, '\n'.join(lines[:12])
        player_rows = [line.split('\t') for line in lines if line.startswith('P\t')]
        event_rows = [line.split('\t') for line in lines if line.startswith('N\t')]
        effect_rows = [line.split('\t') for line in lines if line.startswith('R\t')]
        effect_modulation_rows = [line.split('\t') for line in lines if line.startswith('X\t')]
        filter_rows = [line.split('\t') for line in lines if line.startswith('F\t')]
        modulation_rows = [line.split('\t') for line in lines if line.startswith('G\t')]
        decay_rows = [line.split('\t') for line in lines if line.startswith('E\t')]
        mute_rows = [line.split('\t') for line in lines if line.startswith('M\t')]
        gate_rows = [line.split('\t') for line in lines if line.startswith('C\t')]
        if include_gates:
            return player_rows, event_rows, gate_rows
        if include_mutes:
            return player_rows, mute_rows
        if include_filters:
            return player_rows, filter_rows
        if include_effect_modulations:
            return player_rows, effect_modulation_rows
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

def run_example(name, user_synth_names=(), include_gates=False,
                include_events=False):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read(),
                      user_synth_names=user_synth_names,
                      include_gates=include_gates,
                      include_events=include_events)

assert 'one-shot riser' in run_error_script('Clock.bpm = riser(8)\n')
assert 'riser' in run_error_script('p1 >> saw([0], slide=riser(8))\n')
assert 'riser' in run_error_script('play("x", sample=riser(8))\n')

userSynthDir = os.path.join(sys.argv[2], 'SynthDefs')
userSynthBinary = open(os.path.join(userSynthDir, 'CustomPluck.scsyndef'), 'rb').read()
assert b'CustomPluck' in userSynthBinary
assert b'DexterPluck' not in userSynthBinary
assert not os.path.exists(os.path.join(userSynthDir, 'DexterPluck.scsyndef'))
userSynthRows = run_example('94-user-synthdef-loader.py', ('CustomPluck',))
assert [int(row[6]) for row in userSynthRows] == [58, 58]
for filename, expected_players in (
        ('200-sclork-melodic-starter.py', 3),
        ('201-sclork-drum-kit.py', 3),
        ('202-sclork-guitar-and-pad.py', 3)):
    example_path = os.path.join(sys.argv[2], filename)
    if os.path.isfile(example_path):
        rows = run_example(filename, example_synth_names)
        assert len(rows) == expected_players, (filename, rows)
        assert all(int(row[6]) >= 58 for row in rows), (filename, rows)
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
    graph_count = read('>H')
    graphs = {}
    for _ in range(graph_count):
        name = read_string()
        constant_count = read('>H')
        offset += constant_count * 4
        parameter_count = read('>H')
        offset += parameter_count * 4
        parameter_name_count = read('>H')
        for _ in range(parameter_name_count):
            read_string()
            read('>H')
        ugen_count = read('>H')
        ugens = []
        for _ in range(ugen_count):
            ugen_name = read_string()
            rate = read('>B')
            input_count = read('>H')
            output_count = read('>H')
            read('>h')
            for _ in range(input_count):
                source_index = read('>h')
                read('>h')
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
assert len(legacy) == 1 and len(legacy[0]) == 24
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
assert len(example7) == 2 and all(len(row) == 24 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.0'
example9 = run_example('09-foxdot-core.py')
assert len(example9) == 2 and all(len(row) == 24 for row in example9)
foxSaw, foxPulse = example9
assert foxSaw[1] == '0' and foxSaw[3] == '96.0' and foxSaw[6] == '2'
assert abs(float(foxSaw[7]) - 0.7) < 1e-6 and abs(float(foxSaw[8]) + 0.35) < 1e-6
assert foxPulse[1] == '1' and foxPulse[3] == '96.0' and foxPulse[6] == '3'
assert abs(float(foxPulse[7]) - 0.25) < 1e-6 and abs(float(foxPulse[8]) - 0.35) < 1e-6
example10Rows = run_example('10-pattern-transformations.py')
assert len(example10Rows) == 2 and all(len(row) == 24 for row in example10Rows)
assert example10Rows[0][5] == '7' and example10Rows[0][6] == '2'
assert example10Rows[1][5] == '4' and example10Rows[1][6] == '3'
example11Rows = run_example('11-foxdot-pattern-literals.py')
assert len(example11Rows) == 2 and all(len(row) == 24 for row in example11Rows)
assert example11Rows[0][5] == '7' and example11Rows[1][5] == '2'
example12Rows = run_example('12-pattern-arithmetic.py')
assert len(example12Rows) == 2 and all(len(row) == 24 for row in example12Rows)
assert example12Rows[0][5] == '5' and example12Rows[1][5] == '5'
example13Rows = run_example('13-pattern-methods.py')
assert len(example13Rows) == 5 and all(len(row) == 24 for row in example13Rows)
assert [row[5] for row in example13Rows] == ['8', '5', '4', '4', '4']
example14Rows = run_example('14-pattern-composition.py')
assert len(example14Rows) == 2 and all(len(row) == 24 for row in example14Rows)
assert [row[5] for row in example14Rows] == ['5', '7']
example15Rows = run_example('15-pattern-arithmetic-operators.py')
assert len(example15Rows) == 4 and all(len(row) == 24 for row in example15Rows)
assert [row[5] for row in example15Rows] == ['4', '2', '4', '4']
example16Rows = run_example('16-bounded-pattern-slices.py')
assert len(example16Rows) == 3 and all(len(row) == 24 for row in example16Rows)
assert [row[5] for row in example16Rows] == ['8', '4', '2']
example17Rows = run_example('17-live-player-edits.py')
assert len(example17Rows) == 2 and [row[1] for row in example17Rows] == ['0', '1']
example18Rows = run_example('18-ui-mouse-controls.py')
assert len(example18Rows) == 2 and all(len(row) == 24 for row in example18Rows)
assert example18Rows[0][12] == '1' and example18Rows[0][13] == '0'
assert example18Rows[1][12] == '0' and example18Rows[1][13] == '1'
envelopeRows = run_example('19-envelope-controls.py')
assert len(envelopeRows) == 2 and all(len(row) == 24 for row in envelopeRows)
assert float(envelopeRows[0][14]) == 0.005 and float(envelopeRows[0][15]) == 0.2
assert float(envelopeRows[1][14]) == 0.04 and float(envelopeRows[1][15]) == 0.1
timevarRows = run_example('20-timevar-controls.py')
assert len(timevarRows) == 2 and all(len(row) == 24 for row in timevarRows)
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
foxdotEchoRows, foxdotDecayRows = run_script('p1 >> saw([0, 2, 4, 6], dur=0.5, echo=0.5, decay=0.75, echomix=0.4, feedback=0.5)\n', include_decay=True)
assert foxdotDecayRows[0][0] == 'E' and float(foxdotDecayRows[0][2]) == 0.75 and foxdotDecayRows[0][3] == '-'
assert abs(float(foxdotEchoRows[0][10]) - 0.5) < 1e-9
assert abs(float(foxdotDecayRows[0][4]) - 0.4) < 1e-9 and float(foxdotDecayRows[0][6]) == 0.5 and foxdotDecayRows[0][8] == '-'
_, varDecayRows = run_script('p1 >> saw([0, 2], echo=0.5, decay=var([0.5, 1.5], [1, 1]), delay=linvar([0.1, 0.4], [1, 1]))\n', include_decay=True)
riserRows = run_script('p1 >> saw([0, 2, 4, 7], dur=0.5, amp=riser(8, 0.05, 0.3), cutoff=riser(8, 300, 9000))\n')
assert len(riserRows) == 1 and riserRows[0][16] == 'r:0.05,0.3:8.0'
assert riserRows[0][18] == 'r:300.0,9000.0:8.0'
dropRows = run_script(
    'p1 >> saw([0, 2, 4, 7], amp=drop(30, 2, high=0.4, low=0.02), '
    'cutoff=drop(30, 2, high=8000, low=400))\n')
assert len(dropRows) == 1 and dropRows[0][16] == 'v:0.4,0.02:30.0,2.0'
assert dropRows[0][18] == 'v:8000.0,400.0:30.0,2.0'
wowRows = run_script(
    'p1 >> saw([0, 2, 4], amp=wow(amount=0.18, center=0.32, period=4, seed=7), '
    'pan=wow(amount=0.65, seed=9))\n')
assert len(wowRows) == 1 and wowRows[0][16].startswith('w:')
assert wowRows[0][17].startswith('w:')
tgRows = run_script(
    'p1 >> play("x-o-", dur=0.25, amp=tg(0.125, high=0.7, low=0.08))\n')
assert len(tgRows) == 1 and tgRows[0][16] == 'v:0.7,0.08:0.125,0.125'
wowEffectPlayers, wowEffectTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", '
    'fx_mix=wow(amount=0.2, center=0.5, seed=3))\n',
    include_effect_modulations=True, user_synth_names=example_synth_names)
assert len(wowEffectPlayers) == 1 and any(
    any('w:' in token for token in row) for row in wowEffectTokens)
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], amp=drop(30, 2), fx_mix=drop(16, 16), '
    'pan=wow(seed=1), cutoff=tg())\n')
riserEffectPlayers, riserEffectTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", fx_mix=riser(8, 0.1, 0.8))\n',
    include_effect_modulations=True, user_synth_names=example_synth_names)
assert len(riserEffectPlayers) == 1 and any(
    'r:0.1,0.8:8.0' in row for row in riserEffectTokens)
dropEffectPlayers, dropEffectTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", fx_mix=drop(16, 16))\n',
    include_effect_modulations=True, user_synth_names=example_synth_names)
assert len(dropEffectPlayers) == 1 and any(
    'v:1.0,0.0:16.0,16.0' in row for row in dropEffectTokens)
assert varDecayRows[0][3] == 'v:0.5,1.5:1.0,1.0' and len(varDecayRows[0]) == 9
assert varDecayRows[0][8] == 'l:0.1,0.4:1.0,1.0'
assert len(effectRows) == 1 and len(effectRows[0]) == 24 and len(effectEvents) == 4
assert effectRows[0][19] == 'v:0.1,0.5:1.0,1.0'
assert effectRows[0][20] == 'l:0.1,0.5:1.0,1.0'
assert abs(float(effectEvents[0][7]) - 0.0) < 1e-9
assert abs(float(effectEvents[1][7])) < 1e-9
assert abs(float(effectEvents[2][7])) < 1e-9
assert abs(float(effectEvents[0][5]) - 0.01) < 1e-9 and abs(float(effectEvents[0][6]) - 0.02) < 1e-9
assert abs(float(effectEvents[1][5]) - 0.01) < 1e-9 and abs(float(effectEvents[1][6]) - 0.06) < 1e-9
assert abs(float(effectEvents[2][5]) - 0.03) < 1e-9 and abs(float(effectEvents[2][6]) - 0.1) < 1e-9
riserExample = run_example('209-limut-riser-automation.py')
assert len(riserExample) == 2 and all(len(row) == 24 for row in riserExample)
assert riserExample[0][16] == 'r:0.04,0.28:16.0'
assert riserExample[0][18] == 'r:400.0,9000.0:16.0'
dropExample = run_example('210-limut-drop-patterns.py')
assert len(dropExample) == 2 and all(len(row) == 24 for row in dropExample)
assert dropExample[0][16] == 'v:0.3,0.02:30.0,2.0'
assert dropExample[0][18] == 'v:8000.0,500.0:30.0,2.0'
wowTgExample = run_example('211-limut-wow-tg-patterns.py')
assert len(wowTgExample) == 2 and all(len(row) == 24 for row in wowTgExample)
assert wowTgExample[0][16].startswith('w:')
assert wowTgExample[1][16] == 'v:0.7,0.08:0.125,0.125'
jazzSynthNames = (
    'JazzKitKick', 'JazzKitBrushKick', 'JazzKitBrushTap',
    'JazzKitBrushSweep', 'JazzKitClosedHat', 'JazzKitOpenHat',
    'JazzKitRide', 'JazzKitCrossStick', 'JazzKitRackTom',
    'JazzKitFloorTom')
for jazz_name in jazzSynthNames:
    synthdef_path = os.path.join(userSynthDir, jazz_name + '.scsyndef')
    controls_path = os.path.join(userSynthDir, jazz_name + '.controls')
    assert os.path.isfile(synthdef_path), 'missing compiled Jazz Kit voice: ' + jazz_name
    assert jazz_name.encode('ascii') in open(synthdef_path, 'rb').read()
    assert os.path.isfile(controls_path), 'missing Jazz Kit slider sidecar: ' + jazz_name
jazz214Rows = run_example('214-jazz-brush-kit.py', example_synth_names)
jazz217Rows = run_example('217-jazz-kit-core-groove.py', example_synth_names)
jazz218Rows = run_example('218-jazz-kit-kick-compare.py', example_synth_names)
assert len(jazz214Rows) == 8 and len(jazz217Rows) == 8 and len(jazz218Rows) == 8
jazzDemoSource = ''.join(
    open(os.path.join(sys.argv[2], filename), encoding='utf-8').read()
    for filename in ('214-jazz-brush-kit.py',
                     '217-jazz-kit-core-groove.py',
                     '218-jazz-kit-kick-compare.py'))
assert all('userSynth("' + jazz_name + '"' in jazzDemoSource
           for jazz_name in jazzSynthNames)
swimRows = run_example('219-sardine-swim-finite-cycles.py', example_synth_names)
assert len(swimRows) == 2 and [int(row[5]) for row in swimRows] == [32, 32]
assert [int(row[-1]) for row in swimRows] == [1, 1]
branchRows = run_example('220-sardine-swim-cycle-branches.py',
                         example_synth_names)
assert len(branchRows) == 2 and [int(row[-1]) for row in branchRows] == [1, 1]
cycleGateRows, cycleGateEvents = run_example(
    '222-sardine-swim-every-cycle.py',
    example_synth_names, include_events=True)
assert len(cycleGateRows) == 1 and int(cycleGateRows[0][5]) == 64
assert len(cycleGateEvents) == 64
cycleGatePitches = [int(event[3]) for event in cycleGateEvents]
assert cycleGatePitches[0:4] == cycleGatePitches[4:8]
assert cycleGatePitches[8:12] == cycleGatePitches[0:4]
assert cycleGatePitches[12:16] != cycleGatePitches[0:4]
assert cycleGatePitches[28:32] == cycleGatePitches[12:16]
assert cycleGatePitches[32:36] == cycleGatePitches[0:4]
gatedSwimRows, gatedSwimEvents, gatedSwimControls = run_example(
    '221-sardine-swim-chance-bar-gates.py', example_synth_names,
    include_gates=True)
assert [int(row[5]) for row in gatedSwimRows] == [16, 64]
assert len(gatedSwimControls) == 2
assert [int(row[1]) for row in gatedSwimControls] == [0, 1]
assert [float(row[2]) for row in gatedSwimControls] == [0.5, 0.75]
assert [int(row[4]) for row in gatedSwimControls] == [4, 2]
assert [int(row[5]) for row in gatedSwimControls] == [5, 1]
assert all(0 <= int(row[3]) <= 0xffffffff for row in gatedSwimControls)
repeatable_swim = """Seed.default = 221
@swim(cycles=4, period=1)
def repeatable(p=1, i=0):
    p1 >> bass([0], dur=1, oct=5, chance=0.5, loaf=4, on=(1, 3))
    again(repeatable, p=p, i=i + 1)
"""
_, _, repeatableGatesA = run_script(repeatable_swim, include_gates=True)
_, _, repeatableGatesB = run_script(repeatable_swim, include_gates=True)
assert repeatableGatesA == repeatableGatesB
every_swim = """@swim(cycles=8, period=1)
def every_test(p=1, i=0):
    if every(4):
        p1 >> saw([7], dur=1)
    else:
        p1 >> saw([0], dur=1)
    again(every_test, p=p, i=i + 1)
"""
everyRows, everyEvents = run_script(every_swim, include_events=True)
assert len(everyRows) == 1 and len(everyEvents) == 8
everyPitches = [int(event[3]) for event in everyEvents]
assert everyPitches[0] != everyPitches[1]
assert everyPitches[1] == everyPitches[2] == everyPitches[3]
assert everyPitches[4] == everyPitches[0]
assert everyPitches[7] == everyPitches[3]
every_offset_swim = """@swim(cycles=8, period=1)
def every_offset_test(p=1, i=0):
    if every(4, offset=2):
        p1 >> saw([7], dur=1)
    else:
        p1 >> saw([0], dur=1)
    again(every_offset_test, p=p, i=i + 1)
"""
_, everyOffsetEvents = run_script(
    every_offset_swim, include_events=True)
everyOffsetPitches = [int(event[3]) for event in everyOffsetEvents]
assert everyOffsetPitches[2] != everyOffsetPitches[0]
assert everyOffsetPitches[3] == everyOffsetPitches[0]
variable_timing_swim = """@swim(cycles=4, period=0.5)
def variable_timing(p=0.5, i=0):
    p1 >> saw([i], dur=0.25)
    again(variable_timing, p=p * 2, i=i + 1)
"""
variableTimingRows, variableTimingEvents = run_script(
    variable_timing_swim, include_events=True)
assert len(variableTimingRows) == 1
assert [float(event[1]) for event in variableTimingEvents] == [
    0.0, 0.5, 1.5, 3.5]
periods_timing_swim = """@swim(cycles=8, periods=P[0.5, 1, 0.25, 0.75])
def periods_timing(p=0.5, i=0):
    p1 >> saw([i], dur=0.125)
    again(periods_timing, i=i + 1)
"""
periodsTimingRows, periodsTimingEvents = run_script(
    periods_timing_swim, include_events=True)
assert len(periodsTimingRows) == 1
assert [float(event[1]) for event in periodsTimingEvents] == [
    0.0, 0.5, 1.5, 1.75, 2.5, 3.0, 4.0, 4.25]
sleep_timing_swim = """@swim(cycles=3, period=1)
def sleep_timing(p=1, i=0):
    p1 >> saw([i], dur=0.25)
    sleep(0.5)
    p2 >> sine([i + 3], dur=0.25)
    again(sleep_timing, p=p, i=i + 1)
"""
sleepTimingRows, sleepTimingEvents = run_script(
    sleep_timing_swim, include_events=True)
assert len(sleepTimingRows) == 2 and len(sleepTimingEvents) == 6
assert [float(event[1]) for event in sleepTimingEvents[:3]] == [
    0.0, 1.0, 2.0]
assert [float(event[1]) for event in sleepTimingEvents[3:]] == [
    0.5, 1.5, 2.5]
assert 'sleep() is available only inside an active @swim function' in \
       run_error_script('sleep(0.25)\n')
bad_sleep_timing = """@swim(cycles=2, period=0.5)
def bad_sleep(p=0.5, i=0):
    sleep(0.4)
    sleep(0.2)
    p1 >> saw([i], dur=0.25)
    again(bad_sleep, p=p, i=i + 1)
"""
badSleepError = run_error_script(bad_sleep_timing)
assert 'sleep offsets may not exceed the current cycle period' in \
       badSleepError, badSleepError
outsideSleepCheck = run_compatibility_check('sleep(0.25)\n')
assert 'sleep() is supported only inside a bounded @swim function' in \
       outsideSleepCheck, outsideSleepCheck
many_sleep_calls = '\n'.join(['    sleep(0)'] * 33)
tooManySleepCheck = run_compatibility_check(
    '@swim(cycles=2, period=1)\n'
    'def too_many_sleeps(p=1, i=0):\n' +
    many_sleep_calls + '\n'
    '    p1 >> saw([0], dur=0.25)\n')
assert '@swim supports at most 32 sleep() calls' in tooManySleepCheck, \
       tooManySleepCheck
die_swim = """@swim(cycles=4, period=1)
def named_swim(p=1, i=0):
    p1 >> saw([i], dur=1)
    again(named_swim, p=p, i=i + 1)
p2 >> sine([9], dur=1)
@die
def named_swim():
    pass
"""
dieRows, dieEvents = run_script(die_swim, include_events=True)
assert len(dieEvents) == 1 and int(dieEvents[0][3]) == 76, (
    '@die must stop only the players owned by the named @swim')
stale_die = """@swim(cycles=2, period=1)
def replaced_swim(p=1, i=0):
    p1 >> saw([i], dur=1)
    again(replaced_swim, p=p, i=i + 1)
p1 >> sine([7], dur=1)
@die
def replaced_swim():
    pass
"""
staleDieError = run_error_script(stale_die)
assert 'No current bounded @swim schedule named replaced_swim to stop' in \
       staleDieError, staleDieError
unknown_die = run_error_script("""@die
def never_scheduled():
    pass
""")
assert 'No current bounded @swim schedule named never_scheduled to stop' in \
       unknown_die, unknown_die
too_many_periods = ', '.join(['0.5'] * 33)
for invalid_periods_script, expected_period_error in (
        ("""@swim(cycles=2, periods=[])
def empty_periods(p=1, i=0):
    p1 >> saw([0], dur=1)
""", '@swim periods must be a literal P[...]'),
        ("""@swim(cycles=2, periods=P[0])
def zero_period(p=1, i=0):
    p1 >> saw([0], dur=1)
""", '@swim periods must be a literal P[...]'),
        ("""@swim(cycles=2, periods=P[4097])
def oversized_period(p=1, i=0):
    p1 >> saw([0], dur=1)
""", '@swim periods must be a literal P[...]'),
        ("""@swim(cycles=2, periods=P[p])
def dynamic_period(p=1, i=0):
    p1 >> saw([0], dur=1)
""", '@swim periods must be a literal P[...]'),
        ("""@swim(cycles=2, period=1, periods=P[0.5])
def conflicting_periods(p=1, i=0):
    p1 >> saw([0], dur=1)
""", '@swim period and periods cannot be specified together'),
        ('@swim(cycles=2, periods=P[' + too_many_periods + '])\n'
         'def too_many_period_values(p=1, i=0):\n'
         '    p1 >> saw([0], dur=1)\n',
         '@swim periods must be a literal P[...]')):
    periodError = run_compatibility_check(invalid_periods_script)
    assert expected_period_error in periodError, periodError
for invalid_sleep_script in (
        """@swim(cycles=2, period=1)
def variable_sleep(p=1, i=0):
    sleep(i / 4)
    p1 >> saw([0], dur=0.25)
""",
        """@swim(cycles=2, period=1)
def oversized_sleep(p=1, i=0):
    sleep(4097)
    p1 >> saw([0], dur=0.25)
"""):
    sleepError = run_compatibility_check(invalid_sleep_script)
    assert '@swim sleep() requires one literal beat value from 0 to 4096' \
           in sleepError, sleepError
for invalid_every_script, expected_every_error in (
        ("""@swim(cycles=2, period=1)
def invalid_every_zero(p=1, i=0):
    if every(0):
        p1 >> saw([0], dur=1)
    again(invalid_every_zero, p=p, i=i + 1)
""", '@swim every count must be from 1 to 32 cycles'),
        ("""@swim(cycles=2, period=1)
def invalid_every_offset(p=1, i=0):
    if every(4, offset=4):
        p1 >> saw([0], dur=1)
    again(invalid_every_offset, p=p, i=i + 1)
""", '@swim every offset must be within its cycle count'),
        ("""@swim(cycles=2, period=1)
def invalid_every_dynamic(p=1, i=0):
    if every(i):
        p1 >> saw([0], dur=1)
    again(invalid_every_dynamic, p=p, i=i + 1)
""", '@swim every count must be from 1 to 32 cycles'),
        ('every(4)\n', 'every() is available only inside an active @swim function')):
    everyError = run_error_script(invalid_every_script)
    assert expected_every_error in everyError, everyError
previous_swim_context = namespace['_dexter_swim_context']
namespace['_dexter_swim_context'] = {'iteration': 5}
try:
    assert namespace['every'](4, 1) is True
    for invalid_every_runtime in (
            (0, 0), (33, 0), (True, 0), (4, 4), (4, True), (4, 1.5)):
        try:
            namespace['every'](*invalid_every_runtime)
        except (TypeError, ValueError):
            pass
        else:
            raise AssertionError(
                '@swim every must reject invalid counts and offsets')
finally:
    namespace['_dexter_swim_context'] = previous_swim_context
for swim_fragment in (
        'def every(count, offset=0):',
        'every() is supported only in a bounded @swim if condition.',
        '@swim every() requires a literal count from 1 to 32 and an offset within that count.',
        "return context['iteration'] % count == offset"):
    if swim_fragment not in source:
        raise SystemExit('Bounded Sardine @swim cycle gates are missing: ' +
                         swim_fragment)
invalid_swim_gate = run_error_script("""@swim(cycles=2, period=1)
def invalid_gate(p=1, i=0):
    p1 >> bass([0], dur=1, chance=1.1)
    again(invalid_gate, p=p, i=i + 1)
""")
assert 'chance probability must be from 0 to 1' in invalid_swim_gate
for invalid_gate_script, expected_gate_error in (
        ("""@swim(cycles=2, period=1)
def invalid_loaf(p=1, i=0):
    p1 >> bass([0], dur=1, loaf=33)
    again(invalid_loaf, p=p, i=i + 1)
""", 'loaf must be from 1 to 32 bars'),
        ("""@swim(cycles=2, period=1)
def invalid_on(p=1, i=0):
    p1 >> bass([0], dur=1, loaf=4, on=(0, 3))
    again(invalid_on, p=p, i=i + 1)
""", 'on bar positions must be within the loaf')):
    gate_error = run_error_script(invalid_gate_script)
    assert expected_gate_error in gate_error, gate_error
for swim_fragment in (
        'def again(function, p=None, i=None):',
        'def die(function):',
        'def _dexter_compile_swim(function, cycles=8, period=None, periods=None):',
        "'periods': period_values",
        '@die must be the function’s only bare decorator.',
        '_dexter_unregister_swim_player(player_index)',
        'def _swim_condition_test(self, node, depth=0, budget=None):',
        '@swim may assign each player only once per cycle',
        '@swim is limited to 256 expanded events per player',
        'Chance controls allocate a seed for each PlayerPattern construction.',
        '@swim conditions may use only bounded integer expressions based on i.',
        'update[-5] = 1'):
    if swim_fragment not in source:
        raise SystemExit('Bounded Sardine @swim support is missing: ' +
                         swim_fragment)
jazzCatalogSource = open(
    os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])),
                 'editor_catalog.h'),
    encoding='utf-8').read()
assert 'JazzKitBrushKick' in jazzCatalogSource and 'Brush kick' in jazzCatalogSource
effectExample = run_example('22-timevar-effects-envelopes.py')
assert len(effectExample) == 2 and all(len(row) == 24 for row in effectExample)
reverbRows, reverbEffects = run_script(open(os.path.join(sys.argv[2], '23-reverb-algorithms.py'), encoding='utf-8').read(), include_effects=True)
assert len(reverbRows) == 3 and len(reverbEffects) == 3
assert [int(row[2]) for row in reverbEffects] == [1, 2, 3]
assert [float(row[4]) for row in reverbEffects] == [0.65, 0.75, 0.8]
assert [float(row[7]) for row in reverbEffects] == [15.0, 15.0, 25.0]
assert all(abs(float(row[3]) - expected) < 1e-6 for row, expected in zip(reverbEffects, [0.2, 0.3, 0.25]))
filterRows, filterControls = run_script('p1 >> saw([0, 2, 4, 6], lpf=linvar([300, 3000], [2, 2]), lpr=var([0.35, 0.7], [2, 2]), hpf=var([0, 150], [2, 2]), hpr=0.8, bpf=linvar([500, 1200], [2, 2]), bpr=0.5)\n', include_filters=True)
assert len(filterRows) == 1 and len(filterRows[0]) == 24
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
assert 'Root.default only supports .set(root)' in run_compatibility_check('Root.default.notes\n')
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
assert 'foxDotEnvelopeDurationSeconds' in processor_source and 'safeSynthFrequency' in processor_source
assert 'synthIndex == 5 || synthIndex == 22' in processor_source
assert 'bool oneShot = false;' in processor_source
assert "const bool oneShot = token[0] == 'r';" in processor_source
assert 'if (control.oneShot)' in processor_source
assert 'valueCount != 2 || durationCount != 1' in processor_source
assert "(oneShot && (valueCount != 2 || durationCount != 1))" in processor_source
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw(P[I, V7], strum=0.06)\np2 >> saw(P[I7, V7], strum=var([0.02, 0.05], 2))\np1.mute()\np1.unmute()\n')
muteRows, muteCommands = run_script('p1.mute()\np2.unmute()\n', include_mutes=True)
assert muteRows == [] and muteCommands == [['M', '0', '1'], ['M', '1', '0']]
namespace['_dexter_events'] = []
namespace['_dexter_length'] = 1.0
namespace['_dexter_slots'] = 0
namespace['play']([(60, 64), (64, 67)], step=1, duration=1, strum=0.2)
assert len(namespace['_dexter_events']) == 4, 'same-pitch sustained overlaps are valid voices'
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
assert len(example8) == 2 and all(len(row) == 24 for row in example8)
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
assert example37Modulations[2][6].startswith('v:') and example37Modulations[2][7].startswith('l:')
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
for bassExample in ('39-foxdot-native-bass-varsaw.py',
                    '219-sardine-swim-finite-cycles.py'):
    bassText = open(os.path.join(sys.argv[2], bassExample),
                    encoding='utf-8').read()
    bassTree = ast.parse(bassText, filename=bassExample)
    bassCalls = [node for node in ast.walk(bassTree)
                 if isinstance(node, ast.Call) and
                 isinstance(node.func, ast.Name) and node.func.id == 'bass']
    assert bassCalls, (bassExample, 'missing bass player')
    for bassCall in bassCalls:
        bassKeywords = {item.arg: item.value for item in bassCall.keywords}
        assert isinstance(bassKeywords.get('oct'), ast.Constant) and \
            bassKeywords['oct'].value >= 5, (
                bassExample, 'bass octave is too low after the graph /4')
        assert isinstance(bassKeywords.get('amp'), ast.Constant) and \
            bassKeywords['amp'].value >= 0.35, (
                bassExample, 'bass amplitude is too quiet for the demo')
bassDemoDirectory = os.path.join(os.path.dirname(os.path.abspath(sys.argv[2])))
for bassPath in [os.path.join(bassDemoDirectory, name)
                 for name in os.listdir(bassDemoDirectory)
                 if name.endswith('.py')]:
    bassTree = ast.parse(open(bassPath, encoding='utf-8').read(),
                         filename=os.path.basename(bassPath))
    for bassCall in (node for node in ast.walk(bassTree)
                     if isinstance(node, ast.Call) and
                     isinstance(node.func, ast.Name) and
                     node.func.id in ('bass', 'jbass')):
        bassKeywords = {item.arg: item.value for item in bassCall.keywords}
        octave = bassKeywords.get('oct', ast.Constant(value=5))
        assert isinstance(octave, ast.Constant) and octave.value >= 5, (
            os.path.basename(bassPath),
            'bass/jbass demo is below the audible register after /4')
bassGraphSource = open(os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), '..', 'direct-world',
    'testdata', 'generate_tone.py'), encoding='utf-8').read()
assert 'FoxDot bass frequency / 4' in bassGraphSource
print('PASS bass examples keep the /4 oscillator in an audible register')
example40Rows = run_example('40-charm-and-spark.py')
assert [int(row[6]) for row in example40Rows] == [22, 23]
example41Rows = run_example('41-blip-oscillator.py')
assert [int(row[6]) for row in example41Rows] == [24, 22]
example42Rows = run_example('42-self-contained-ugens.py')
assert [int(row[6]) for row in example42Rows] == [25, 26, 27, 28, 29, 30]
assert namespace['bug']([0]).synthRate == 1.0
supported_report = run_compatibility_check('Clock.bpm = 120\nScale.default.set("minor")\np1 >> saw([0, 2, 4], dur=0.5, oct=5, sus=0.4, amp=0.7, pan=-0.2, echo=0.5, decay=0.75)\np2 >> pulse([0, None, 3], dur=1, cutoff=3000)\np3 >> saw(P[0, None, 2], dur=0.5)\np4 >> triangle([0, 2], pan=MouseX(), cutoff=MouseY(), atk=0.02, rel=0.15)\np5 >> saw([0, 2], amp=var([0.2, 0.6], [4, 4]), pan=linvar([-0.8, 0.8], 8), cutoff=linvar([400, 5000], 8))\np1 >> saw(P[0:8:2], dur=0.5)\np6 >> saw([0, 2], lpf=var([400, 5000], 4), lpr=0.45, hpf=300, hpr=0.8, bpf=1200, bpr=0.35, bpnoise=0.3, swell=1.0)\np1.stop()\n')
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw([0, 2], amp=riser(8, 0.05, 0.4), cutoff=riser(8, 300, 5000))\n')
assert 'No unsupported constructs detected' in supported_report, supported_report
assert 'script was not executed' in supported_report, supported_report
loadedUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("CustomPluck", P[0, 2, 4], dur=0.5, amp=0.3)\n',
    ('CustomPluck',))
assert 'No unsupported constructs detected' in loadedUserSynthReport, loadedUserSynthReport
missingUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("NotInstalled", [0, 2], dur=0.5)\n')
assert 'not present in Portable/SynthDefs' in missingUserSynthReport
unsupported_report = run_compatibility_check('from FoxDot import *\nClock.swing = 0.2\np9 >> bass([0, 2], dur=0.5)\nplay("x-o-")\nP["x-o"]\nP[0:]\nP[0:257]\nP[0:8:0]\nP[start:8]\nP[0] | ["x-o"]\n')
for phrase in ('Line 1:', 'Line 2:', 'Line 3:', 'Line 5:', 'finite stop', '256 generated items', 'step cannot be zero', 'integer-literal bounds', 'Pattern | and & do not accept sample strings'):
    assert phrase.lower() in unsupported_report.lower(), (phrase, unsupported_report)
syntax_report = run_compatibility_check('p1 >> saw([0])\nif :\n')
assert 'Python syntax error at column' in syntax_report and 'Line 2:' in syntax_report, syntax_report
for filename in os.listdir(sys.argv[2]):
    if filename.endswith('.py'):
        example_report = run_compatibility_check(
            open(os.path.join(sys.argv[2], filename), encoding='utf-8').read(),
            example_synth_names)
        assert 'No unsupported constructs detected' in example_report, (filename, example_report)
for scale_name in namespace['_SCALES']:
    namespace['Scale'].default.set(scale_name)
    assert namespace['Scale'].__dict__[scale_name] == scale_name

# Keep the custom-SynthDef mono/stereo routing contract tied to the engine
# implementation. The small C++ unit test exercises the width/bus/pan helpers;
# these checks ensure the processor uses both channel layouts end-to-end.
processor_path = os.path.join(os.path.dirname(__file__), '..', 'processor.cpp')
processor_source = open(processor_path, encoding='utf-8').read()
routing_header_path = os.path.join(os.path.dirname(__file__), '..',
                                   'user_synth_routing.h')
routing_header = open(routing_header_path, encoding='utf-8').read()
assert 'synthOutputChannels(loaded)' in processor_source
assert 'mergeSynthOutputChannels' in processor_source
assert 'userDefinition.outputChannels = synthOutputChannels(loaded)' in processor_source
assert 'userDefinition.outputChannels = 2' in processor_source
assert 'kStereoSynthOutputBaseBus' in processor_source
assert 'sourceRight = world_->mAudioBus' in processor_source
assert 'voice.lpfRightState' in processor_source
assert 'stereoBalanceGains(voice.pan)' in processor_source
assert 'synthOutputChannelsFromOutInputCount' in routing_header
assert 'mergeSynthOutputChannels' in routing_header
assert 'userSynthOutputBus' in routing_header
print('Smoke tests passed: embedded runtime, pattern API, static FoxDot compatibility checks, line diagnostics, all Portable examples, user SynthDef mono/stereo routing, and Example 94.')
