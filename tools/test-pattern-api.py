import ast
import builtins
import os
import re
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
assert namespace['transpose']([60, None, [64, 67]], 12) == [72, None, [76, 79]]
euclidean = namespace['euclid'](3, 8, 36, 1)
assert len(euclidean) == 8 and euclidean.count(36) == 3
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
        assert lines[0] == 'OK\t2' or lines[0] == 'OK\t1', lines[0]
        return [line.split('\t') for line in lines if line.startswith('P\t')]

def run_example(name):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read())

legacy = run_script('tempo(120)\nplay([60, 64], step=0.5, duration=0.4)\n')
assert len(legacy) == 1 and len(legacy[0]) == 12
assert legacy[0][5] == '2' and legacy[0][6] == '0'
assert legacy[0][7] == '1.0' and legacy[0][8] == '0.0'
assert legacy[0][10] == '0.0' and legacy[0][11] == '0.25'
example7 = run_example('07-independent-players.py')
assert len(example7) == 2 and all(len(row) == 12 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.25'
example8 = run_example('08-synths-and-controls.py')
assert len(example8) == 2 and all(len(row) == 12 for row in example8)
p1, p2 = example8
assert p1[1] == '0' and p1[6] == '1' and abs(float(p1[7]) - 0.7) < 1e-6
assert abs(float(p1[8]) + 0.4) < 1e-6 and p1[9] == '5000.0'
assert p1[10] == '0.0' and p1[11] == '0.25'
assert p2[1] == '1' and p2[6] == '2' and abs(float(p2[7]) - 0.5) < 1e-6
assert abs(float(p2[8]) - 0.4) < 1e-6 and p2[9] == '1800.0'
assert p2[10] == '0.25' and p2[11] == '0.24'
print('Smoke tests passed: wrapper execution and complete player-record serialization, patterns, oscillator modes, audio controls, and Examples 07/08.')
