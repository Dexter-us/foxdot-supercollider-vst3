import builtins
import sys

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
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_updates': {},
    '_dexter_default_touched': False, '__name__': 'dexter_test',
}
safe_names = ('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
              'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
              'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str',
              'sum', 'tuple', 'zip', 'ValueError', 'TypeError',
              'ZeroDivisionError', '__build_class__')
namespace['__builtins__'] = {name: getattr(builtins, name) for name in safe_names}
exec(source[start:end], namespace, namespace)
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
namespace['p1'] >> namespace['sequence']([60, None, 64], step=0.5, duration=0.4)
namespace['p2'] >> namespace['sequence']([0, 2, 4], step=1.0, degrees=True)
assert set(namespace['_dexter_updates']) == {0, 1}
assert namespace['_dexter_updates'][0][0] is False
assert len(namespace['_dexter_updates'][0][3]) == 2
assert namespace['_dexter_updates'][1][3][1][2] == 64
namespace['p1'].stop()
assert namespace['_dexter_updates'][0][0] is True
namespace['_dexter_updates'].clear()
namespace['p1'] >> namespace['sequence']([67], step=1.0)
assert set(namespace['_dexter_updates']) == {0}
namespace['_dexter_updates'].clear()
example = open(sys.argv[2] + '/07-independent-players.py', encoding='utf-8').read()
exec(compile(example, '07-independent-players.py', 'exec'), namespace, namespace)
assert set(namespace['_dexter_updates']) == {0, 1}
assert len(namespace['_dexter_updates'][0][3]) == 6
assert len(namespace['_dexter_updates'][1][3]) == 2
print('Pattern API smoke tests passed: transforms, legacy play, independent players, stop, and Portable example 07.')
