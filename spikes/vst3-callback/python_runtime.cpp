#include "python_runtime.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
std::atomic<unsigned long long> gScriptNumber{0};

#if defined(_WIN32)
using PySetPythonHome = void(__cdecl*)(const wchar_t*);
using PyInitializeEx = void(__cdecl*)(int);
using PyIsInitialized = int(__cdecl*)();
using PyEvalSaveThread = void*(__cdecl*)();
using PyRunSimpleStringFlags = int(__cdecl*)(const char*, void*);
using PyGilStateEnsure = int(__cdecl*)();
using PyGilStateRelease = void(__cdecl*)(int);

struct PythonApi {
    HMODULE module = nullptr;
    PySetPythonHome setPythonHome = nullptr;
    PyInitializeEx initializeEx = nullptr;
    PyIsInitialized isInitialized = nullptr;
    PyEvalSaveThread evalSaveThread = nullptr;
    PyRunSimpleStringFlags runSimpleStringFlags = nullptr;
    PyGilStateEnsure gilStateEnsure = nullptr;
    PyGilStateRelease gilStateRelease = nullptr;
};

PythonApi gPython;
std::mutex gPythonMutex;

template <typename T>
bool loadFunction(HMODULE module, const char* name, T& function) {
    function = reinterpret_cast<T>(GetProcAddress(module, name));
    return function != nullptr;
}
#endif

std::string pythonLiteral(const std::string& value) {
    std::string result = "'";
    result.reserve(value.size() + 2);
    for (const char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '\'':
            result += "\\'";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        default:
            result += character;
            break;
        }
    }
    result += '\'';
    return result;
}

std::string pathAsUtf8(const std::filesystem::path& path) {
#if defined(_WIN32)
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
#else
    return path.string();
#endif
}

std::string makeEvaluationScript(const std::string& sourcePath,
                                const std::string& resultPath,
                                const std::string& namespaceName) {
    static constexpr char api[] = R"PY(
def tempo(bpm):
    global _dexter_bpm
    bpm = float(bpm)
    if bpm != bpm or not 20.0 <= bpm <= 300.0:
        raise ValueError('tempo must be from 20 to 300 BPM')
    _dexter_bpm = bpm

class _FoxDotClock:
    def __getattr__(self, name):
        if name == 'bpm':
            return _dexter_bpm
        if name == 'meter':
            return (4, 4)
        raise ValueError('Clock.' + str(name) + ' is not supported in the embedded clock')
    def __setattr__(self, name, value):
        if name == 'bpm':
            tempo(value)
            return
        if name == 'meter' and tuple(value) == (4, 4):
            return
        raise AttributeError('Clock.' + str(name) + ' is not supported in the embedded clock')

Clock = _FoxDotClock()

_SCALES = {
    'major': (0, 2, 4, 5, 7, 9, 11),
    'minor': (0, 2, 3, 5, 7, 8, 10),
    'natural_minor': (0, 2, 3, 5, 7, 8, 10),
    'aeolian': (0, 2, 3, 5, 7, 8, 10),
    'dorian': (0, 2, 3, 5, 7, 9, 10),
    'mixolydian': (0, 2, 4, 5, 7, 9, 10),
    'major_pentatonic': (0, 2, 4, 7, 9),
    'minor_pentatonic': (0, 3, 5, 7, 10),
    'blues': (0, 3, 5, 6, 7, 10),
}

def _normalize_scale_name(name):
    return str(name).strip().lower().replace(' ', '_').replace('-', '_')

def scale(name='major', root=60):
    global _dexter_scale, _dexter_root
    name = _normalize_scale_name(name)
    if name not in _SCALES:
        raise ValueError('unknown scale; choose major, minor, dorian, mixolydian, major_pentatonic, minor_pentatonic, or blues')
    root = int(root)
    if not 0 <= root <= 127:
        raise ValueError('scale root must be a MIDI note from 0 to 127')
    _dexter_scale = _SCALES[name]
    _dexter_root = root

class _FoxDotScaleDefault:
    def set(self, name, root=None):
        scale(name, _dexter_root if root is None else root)

class _FoxDotScaleNamespace:
    def __init__(self):
        self.default = _FoxDotScaleDefault()
        self.major = 'major'
        self.minor = 'minor'
        self.dorian = 'dorian'
        self.mixolydian = 'mixolydian'
        self.major_pentatonic = 'major_pentatonic'
        self.minor_pentatonic = 'minor_pentatonic'
        self.blues = 'blues'

Scale = _FoxDotScaleNamespace()

def reverse(pattern):
    return list(reversed(pattern))

def rotate(pattern, amount=1):
    values = list(pattern)
    if not values:
        return values
    amount = int(amount) % len(values)
    return values[-amount:] + values[:-amount] if amount else values

def repeat(pattern, count=2):
    values = list(pattern)
    count = int(count)
    if count < 1 or len(values) * count > 256:
        raise ValueError('repeat must produce between 1 and 256 steps')
    return values * count

def transpose(pattern, interval=1):
    interval = int(interval)
    result = []
    for item in pattern:
        if item is None:
            result.append(None)
        elif isinstance(item, (list, tuple)):
            result.append([int(note) + interval for note in item])
        else:
            result.append(int(item) + interval)
    return result

def euclid(hits, steps, note=60, rotation=0):
    hits = int(hits)
    steps = int(steps)
    note = int(note)
    if steps < 1 or steps > 256 or hits < 0 or hits > steps:
        raise ValueError('euclid requires 0 <= hits <= steps <= 256')
    if not 0 <= note <= 127:
        raise ValueError('euclidean note must be a MIDI pitch from 0 to 127')
    values = []
    accumulator = 0
    for _ in range(steps):
        accumulator += hits
        if accumulator >= steps:
            values.append(note)
            accumulator -= steps
        else:
            values.append(None)
    return rotate(values, rotation)

def _timing_pattern(value, count, name):
    if count == 0:
        return []
    values = [value] if isinstance(value, (int, float)) else list(value)
    if not values:
        raise ValueError(name + ' pattern cannot be empty')
    return [float(values[index % len(values)]) for index in range(count)]

def play(notes, step=1.0, duration=None, velocity=100, offset=0.0, degrees=False):
    global _dexter_length, _dexter_slots, _dexter_default_touched
    _dexter_default_touched = True
    if isinstance(notes, (int, float)):
        notes = [notes]
    if isinstance(notes, str):
        raise ValueError('FoxDot sample strings are not included yet; sample support is deferred')
    notes = list(notes)
    steps = _timing_pattern(step, len(notes), 'dur')
    durations = _timing_pattern(step if duration is None else duration, len(notes), 'sus')
    offset = float(offset)
    velocity = int(velocity)
    if offset != offset or offset < 0.0 or offset > 4096:
        raise ValueError('offset must be between 0 and 4096 beats')
    if any(value != value or value < 0.1 or value > 4096.0 for value in steps):
        raise ValueError('dur values must be between 0.1 and 4096 beats')
    if any(value != value or value < 0.02 or value > stepValue + 1e-9 for value, stepValue in zip(durations, steps)):
        raise ValueError('this embedded first pass requires each sus value to fit within its dur value')
    totalBeats = sum(steps)
    if offset + totalBeats > 4096:
        raise ValueError('offset and pattern length must fit within 4096 beats')
    if not 1 <= velocity <= 127:
        raise ValueError('velocity must be 1..127')
    if _dexter_slots + len(notes) > 256:
        raise ValueError('a pattern is limited to 256 step slots')
    additions = []
    start = offset
    for i, item in enumerate(notes):
        if item is None:
            start += steps[i]
            continue
        chord = list(item) if isinstance(item, (list, tuple)) else [item]
        if len(chord) > 8:
            raise ValueError('a chord can contain at most 8 notes')
        for note in chord:
            note = int(note)
            if degrees:
                octave, degree = divmod(note, len(_dexter_scale))
                note = _dexter_root + octave * 12 + _dexter_scale[degree]
            if not 0 <= note <= 127:
                raise ValueError('notes must resolve to MIDI pitches 0..127')
            additions.append((start, durations[i], note, velocity))
        start += steps[i]
    if len(_dexter_events) + len(additions) > 256:
        raise ValueError('a pattern is limited to 256 note events')
    candidate = _dexter_events + additions
    points = sorted([(beat, 1, note) for beat, dur, note, _ in candidate] +
                    [(beat + dur, -1, note) for beat, dur, note, _ in candidate])
    active = 0
    active_pitches = set()
    for beat, delta, note in points:
        if delta < 0:
            active -= 1
            active_pitches.discard(note)
        else:
            if active >= 8:
                raise ValueError('a pattern can have at most 8 simultaneous notes')
            if note in active_pitches:
                raise ValueError('the same MIDI pitch cannot overlap itself')
            active += 1
            active_pitches.add(note)
    _dexter_events.extend(additions)
    _dexter_slots += len(notes)
    _dexter_length = max(_dexter_length, offset + totalBeats)

class PlayerPattern:
    def __init__(self, notes, step=1.0, duration=None, velocity=100,
                 offset=0.0, degrees=False, synth='sine', amp=1.0, pan=0.0,
                 cutoff=20000.0, echo=0.0, delay=0.25, scaleName=None):
        self.notes = notes
        self.step = step
        self.duration = duration
        self.velocity = velocity
        self.offset = offset
        self.degrees = degrees
        if synth not in ('sine', 'triangle', 'saw', 'square'):
            raise ValueError('synth must be sine, triangle, saw, or square')
        self.synth = synth
        self.scaleName = scaleName
        self.amp = float(amp)
        self.pan = float(pan)
        self.cutoff = float(cutoff)
        self.echo = float(echo)
        self.delay = float(delay)
        if self.amp != self.amp or not 0.0 <= self.amp <= 1.0:
            raise ValueError('amp must be between 0 and 1')
        if self.pan != self.pan or not -1.0 <= self.pan <= 1.0:
            raise ValueError('pan must be between -1 and 1')
        if self.cutoff != self.cutoff or not 20.0 <= self.cutoff <= 20000.0:
            raise ValueError('cutoff must be between 20 and 20000 Hz')
        if self.echo != self.echo or not 0.0 <= self.echo <= 0.75:
            raise ValueError('echo must be between 0 and 0.75')
        if self.delay != self.delay or not 0.05 <= self.delay <= 1.0:
            raise ValueError('delay must be between 0.05 and 1 second')

def sequence(notes, step=1.0, duration=None, velocity=100,
             offset=0.0, degrees=False, synth='sine', amp=1.0, pan=0.0,
             cutoff=20000.0, echo=0.0, delay=0.25):
    return PlayerPattern(notes, step, duration, velocity, offset, degrees,
                        synth, amp, pan, cutoff, echo, delay)

def _fox_wave(synthName, notes=0, dur=1.0, oct=5, sus=None, scale=None,
              amp=1.0, pan=0.0, cutoff=20000.0, echo=0.0, delay=0.25,
              velocity=100, rate=0.0):
    oct = int(oct)
    if not 0 <= oct <= 10:
        raise ValueError('oct must be from 0 to 10 in the embedded FoxDot subset')
    scaleName = None if scale is None else _normalize_scale_name(scale)
    if scaleName is not None and scaleName not in _SCALES:
        raise ValueError('unsupported FoxDot scale: ' + str(scale))
    degreesPerOctave = len(_SCALES[scaleName]) if scaleName is not None else len(_dexter_scale)
    shift = (oct - 5) * degreesPerOctave
    def shift_note(note):
        if note is None:
            return None
        if isinstance(note, (list, tuple)):
            return [shift_note(value) for value in note]
        return int(note) + shift
    if isinstance(notes, (int, float)):
        notes = [notes]
    if isinstance(notes, str):
        raise ValueError('FoxDot sample strings are not included yet; sample support is deferred')
    notes = [shift_note(note) for note in notes]
    return PlayerPattern(notes, dur, dur if sus is None else sus, velocity,
                         0.0, True, synthName, amp, pan, cutoff, echo, delay,
                         scaleName)

def saw(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
        pan=0.0, cutoff=20000.0, echo=0.0, delay=0.25, velocity=100, rate=0.0):
    return _fox_wave('saw', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate)

def pulse(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
          pan=0.0, cutoff=20000.0, echo=0.0, delay=0.25, velocity=100, rate=0.0):
    return _fox_wave('square', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate)

def sine(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
         pan=0.0, cutoff=20000.0, echo=0.0, delay=0.25, velocity=100, rate=0.0):
    return _fox_wave('sine', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate)

def triangle(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
             pan=0.0, cutoff=20000.0, echo=0.0, delay=0.25, velocity=100, rate=0.0):
    return _fox_wave('triangle', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate)

class Player:
    def __init__(self, number):
        self.number = number

    def __rshift__(self, pattern):
        global _dexter_events, _dexter_length, _dexter_slots
        global _dexter_default_touched, _dexter_scale, _dexter_root
        if not isinstance(pattern, PlayerPattern):
            raise TypeError('assign sequence(...) to a player using >>')
        saved = (_dexter_events, _dexter_length, _dexter_slots,
                 _dexter_default_touched, _dexter_scale, _dexter_root)
        _dexter_events = []
        _dexter_length = 1.0
        _dexter_slots = 0
        _dexter_default_touched = False
        try:
            if pattern.scaleName is not None:
                scale(pattern.scaleName, _dexter_root)
            play(pattern.notes, step=pattern.step, duration=pattern.duration,
                 velocity=pattern.velocity, offset=pattern.offset,
                 degrees=pattern.degrees)
            compiledLength = _dexter_length
            compiledEvents = list(_dexter_events)
        finally:
            _dexter_events, _dexter_length, _dexter_slots, _dexter_default_touched = saved[:4]
            _dexter_scale, _dexter_root = saved[4], saved[5]
        playerIndex = self.number - 1
        if not 0 <= playerIndex < 8:
            raise ValueError('player number must be 1..8')
        synthIndex = ('sine', 'triangle', 'saw', 'square').index(pattern.synth)
        _dexter_updates[playerIndex] = (False, _dexter_bpm, compiledLength,
                                        compiledEvents, synthIndex, pattern.amp,
                                        pattern.pan, pattern.cutoff, pattern.echo,
                                        pattern.delay)
        if playerIndex == 0:
            _dexter_default_touched = False
        return self

    def stop(self):
        _dexter_updates[self.number - 1] = (True, _dexter_bpm, 1.0, [], 0, 1.0,
                                             0.0, 20000.0, 0.0, 0.25)

p1 = Player(1)
p2 = Player(2)
p3 = Player(3)
p4 = Player(4)
p5 = Player(5)
p6 = Player(6)
p7 = Player(7)
p8 = Player(8)
)PY";

    std::ostringstream script;
    script << "import builtins, traceback, sys, time\n"
           << "_dexter_scope = globals().setdefault(" << pythonLiteral(namespaceName) << ", {})\n"
           << "_dexter_scope['__name__'] = 'dexter'\n"
           << "_dexter_scope.update({'_dexter_events': [], '_dexter_bpm': 120.0, "
           << "'_dexter_length': 1.0, '_dexter_slots': 0, '_dexter_updates': {}, '_dexter_default_touched': False, "
           << "'_dexter_scale': (0, 2, 4, 5, 7, 9, 11), '_dexter_root': 60})\n"
           << "_dexter_scope['__builtins__'] = {name: getattr(builtins, name) for name in "
           << "('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter', 'float', "
           << "'int', 'isinstance', 'len', 'list', 'map', 'max', 'min', 'range', "
           << "'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum', 'tuple', "
           << "'zip', 'ValueError', 'TypeError', 'ZeroDivisionError', "
           << "'__build_class__')}\n"
           << "exec(compile(" << pythonLiteral(api) << ", '<Dexter API>', 'exec'), "
           << "_dexter_scope, _dexter_scope)\n"
           << "_dexter_deadline = time.monotonic() + 2.0\n"
           << "def _dexter_time_limit(frame, event, arg):\n"
           << "    if time.monotonic() > _dexter_deadline:\n"
           << "        raise TimeoutError('live-code evaluation exceeded 2 seconds')\n"
           << "    return _dexter_time_limit\n"
           << "sys.settrace(_dexter_time_limit)\n"
           << "try:\n"
           << "    try:\n"
           << "        with open(" << pythonLiteral(sourcePath) << ", 'r', encoding='utf-8') as _f:\n"
           << "            _src = _f.read()\n"
           << "        exec(compile(_src, '<Dexter live code>', 'exec'), _dexter_scope, _dexter_scope)\n"
           << "    finally:\n"
           << "        sys.settrace(None)\n"
           << "    with open(" << pythonLiteral(resultPath) << ", 'w', encoding='utf-8') as _f:\n"
            << "        if _dexter_scope['_dexter_default_touched']:\n"
            << "            _dexter_scope['_dexter_updates'][0] = (False, _dexter_scope['_dexter_bpm'], _dexter_scope['_dexter_length'], list(_dexter_scope['_dexter_events']), 0, 1.0, 0.0, 20000.0, 0.0, 0.25)\n"
            << "        _updates = _dexter_scope['_dexter_updates']\n"
            << "        _f.write('OK\\t{}\\n'.format(len(_updates)))\n"
            << "        for _player in sorted(_updates):\n"
            << "            _stopped, _bpm, _length, _events, _synth, _amp, _pan, _cutoff, _echo, _delay = _updates[_player]\n"
            << "            _f.write('P\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\n'.format(_player, int(_stopped), _bpm, _length, len(_events), _synth, _amp, _pan, _cutoff, _echo, _delay))\n"
            << "            for _e in _events:\n"
            << "                _f.write('N\\t{}\\t{}\\t{}\\t{}\\n'.format(*_e))\n"
            << "except BaseException:\n"
           << "    with open(" << pythonLiteral(resultPath) << ", 'w', encoding='utf-8') as _f:\n"
           << "        _f.write('ERROR\\n' + traceback.format_exc())\n";
    return script.str();
}

bool parseResult(const std::filesystem::path& resultPath,
                 PythonPattern& pattern,
                 std::string& error) {
    std::ifstream input(resultPath, std::ios::binary);
    if (!input) {
        error = "Python did not produce a result file.";
        return false;
    }
    std::string header;
    std::getline(input, header);
    if (header == "ERROR") {
        std::ostringstream message;
        message << input.rdbuf();
        error = message.str();
        return false;
    }
    std::istringstream fields(header);
    std::string marker;
    std::size_t updateCount = 0;
    if (!(fields >> marker >> updateCount) || marker != "OK" || updateCount > 8) {
        error = "Python returned an invalid player update list.";
        return false;
    }
    pattern.players.clear();
    pattern.players.reserve(updateCount);
    std::array<bool, 8> seenPlayers{};
    for (std::size_t playerNumber = 0; playerNumber < updateCount; ++playerNumber) {
        char record = 0;
        int stopped = 0;
        std::size_t eventCount = 0;
        PythonPlayerUpdate update;
        if (!(input >> record >> update.playerIndex >> stopped >> update.bpm >>
              update.lengthBeats >> eventCount >> update.synthIndex >> update.amplitude >>
              update.pan >> update.cutoffHz >> update.echoMix >> update.echoDelaySeconds) ||
            record != 'P' || update.playerIndex < 0 || update.playerIndex >= 8 ||
            stopped < 0 || stopped > 1 ||
            seenPlayers[static_cast<std::size_t>(update.playerIndex)] || eventCount > 256 ||
            !std::isfinite(update.bpm) || update.bpm < 20.0 || update.bpm > 300.0 ||
            !std::isfinite(update.lengthBeats) || update.lengthBeats <= 0.0 ||
            update.lengthBeats > 4096.0 || update.synthIndex < 0 || update.synthIndex > 3 ||
            !std::isfinite(update.amplitude) || update.amplitude < 0.0 || update.amplitude > 1.0 ||
            !std::isfinite(update.pan) || update.pan < -1.0 || update.pan > 1.0 ||
            !std::isfinite(update.cutoffHz) || update.cutoffHz < 20.0 ||
            update.cutoffHz > 20000.0 || !std::isfinite(update.echoMix) ||
            update.echoMix < 0.0 || update.echoMix > 0.75 ||
            !std::isfinite(update.echoDelaySeconds) || update.echoDelaySeconds < 0.05 ||
            update.echoDelaySeconds > 1.0) {
            error = "Python returned a malformed player update.";
            return false;
        }
        seenPlayers[static_cast<std::size_t>(update.playerIndex)] = true;
        update.stopped = stopped != 0;
        update.events.reserve(eventCount);
        for (std::size_t i = 0; i < eventCount; ++i) {
            char noteRecord = 0;
            PythonNoteEvent event;
            if (!(input >> noteRecord >> event.beat >> event.durationBeats >>
                  event.midiNote >> event.velocity) || noteRecord != 'N' ||
                !std::isfinite(event.beat) || !std::isfinite(event.durationBeats) ||
                event.beat < 0.0 || event.beat > update.lengthBeats ||
                event.durationBeats < 0.02 ||
                event.beat + event.durationBeats > update.lengthBeats + 1e-9 ||
                event.midiNote < 0 || event.midiNote > 127 ||
                event.velocity < 1 || event.velocity > 127) {
                error = "Python returned a malformed note event.";
                return false;
            }
            update.events.push_back(event);
        }
        std::sort(update.events.begin(), update.events.end(),
                  [](const PythonNoteEvent& a, const PythonNoteEvent& b) {
                      return a.beat < b.beat;
                  });
        struct Boundary { double beat; int note; int delta; };
        std::vector<Boundary> boundaries;
        boundaries.reserve(update.events.size() * 2);
        for (const auto& event : update.events) {
            boundaries.push_back({event.beat, event.midiNote, 1});
            boundaries.push_back({event.beat + event.durationBeats, event.midiNote, -1});
        }
        std::sort(boundaries.begin(), boundaries.end(),
                  [](const Boundary& a, const Boundary& b) {
                      if (a.beat != b.beat) return a.beat < b.beat;
                      return a.delta < b.delta;
                  });
        std::array<bool, 128> activePitches{};
        int activeNotes = 0;
        for (const auto& boundary : boundaries) {
            if (boundary.delta < 0) {
                --activeNotes;
                activePitches[static_cast<std::size_t>(boundary.note)] = false;
            } else {
                const auto pitch = static_cast<std::size_t>(boundary.note);
                if (activeNotes >= 8 || activePitches[pitch]) {
                    error = "Player pattern exceeds polyphony or overlaps a repeated pitch.";
                    return false;
                }
                ++activeNotes;
                activePitches[pitch] = true;
            }
        }
        pattern.players.push_back(std::move(update));
    }
    return true;
}
} // namespace

PortablePython& PortablePython::instance() {
    static PortablePython runtime;
    return runtime;
}

bool PortablePython::initialize(const std::filesystem::path& runtimeDirectory,
                                std::string& error) {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(gPythonMutex);
    if (initialized_)
        return true;

    const auto dllPath = runtimeDirectory / L"python312.dll";
    gPython.module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!gPython.module) {
        error = "Could not load bundled Python 3.12 (python312.dll).";
        return false;
    }

    if (!loadFunction(gPython.module, "Py_SetPythonHome", gPython.setPythonHome) ||
        !loadFunction(gPython.module, "Py_InitializeEx", gPython.initializeEx) ||
        !loadFunction(gPython.module, "Py_IsInitialized", gPython.isInitialized) ||
        !loadFunction(gPython.module, "PyEval_SaveThread", gPython.evalSaveThread) ||
        !loadFunction(gPython.module, "PyRun_SimpleStringFlags", gPython.runSimpleStringFlags) ||
        !loadFunction(gPython.module, "PyGILState_Ensure", gPython.gilStateEnsure) ||
        !loadFunction(gPython.module, "PyGILState_Release", gPython.gilStateRelease)) {
        error = "The bundled Python runtime is missing required embedding functions.";
        return false;
    }

    static std::wstring pythonHome;
    pythonHome = runtimeDirectory.wstring();
    gPython.setPythonHome(pythonHome.c_str());
    gPython.initializeEx(0);
    if (!gPython.isInitialized()) {
        error = "Python failed to initialize from the bundled runtime.";
        return false;
    }

    // The interpreter must not retain the GIL on the thread that initialized
    // it; editor-triggered evaluations may come from a different host thread.
    gPython.evalSaveThread();
    initialized_ = true;
    return true;
#else
    (void)runtimeDirectory;
    error = "The portable embedded Python runtime is currently packaged for Windows.";
    return false;
#endif
}

bool PortablePython::evaluate(const std::filesystem::path& runtimeDirectory,
                             const std::string& namespaceName,
                             const std::string& source,
                             PythonPattern& pattern,
                             std::string& error) {
    if (source.size() > 64 * 1024) {
        error = "Code is limited to 64 KB in this prototype.";
        return false;
    }
    if (!initialize(runtimeDirectory, error))
        return false;

#if defined(_WIN32)
    const auto number = gScriptNumber.fetch_add(1, std::memory_order_relaxed);
    const auto base = std::filesystem::temp_directory_path() /
                      (L"dexter-python-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(number));
    const auto sourcePath = base.wstring() + L".py";
    const auto resultPath = base.wstring() + L".out";

    {
        std::ofstream sourceFile(sourcePath, std::ios::binary);
        if (!sourceFile) {
            error = "Could not create a temporary Python source file.";
            return false;
        }
        sourceFile.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    const auto sourceUtf8 = pathAsUtf8(sourcePath);
    const auto resultUtf8 = pathAsUtf8(resultPath);
    const auto script = makeEvaluationScript(sourceUtf8, resultUtf8, namespaceName);
    int result = -1;
    {
        std::lock_guard<std::mutex> lock(gPythonMutex);
        const int gil = gPython.gilStateEnsure();
        result = gPython.runSimpleStringFlags(script.c_str(), nullptr);
        gPython.gilStateRelease(gil);
    }

    bool success = false;
    if (result == 0)
        success = parseResult(resultPath, pattern, error);
    else
        error = "Python could not evaluate the editor script.";

    std::error_code ignored;
    std::filesystem::remove(sourcePath, ignored);
    std::filesystem::remove(resultPath, ignored);
    return success;
#else
    (void)namespaceName;
    (void)source;
    (void)pattern;
    return false;
#endif
}
