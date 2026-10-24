import sys

if len(sys.argv) != 2:
    raise SystemExit('usage: test-pattern-api.py python_runtime.cpp')
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
    '_dexter_root': 60,
}
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
print('Pattern API smoke tests passed: reverse, rotate, repeat, transpose, euclid, and play.')
