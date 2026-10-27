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

std::string makeCompatibilityScript(const std::string& sourcePath,
                                    const std::string& reportPath) {
    static constexpr char compatibilityChecker[] = R"PY(
import ast
import re

_COMPAT_CALLS = {
    'abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
    'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
    'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum',
    'tuple', 'zip', 'ValueError', 'TypeError', 'ZeroDivisionError',
    'tempo', 'scale', 'reverse', 'rotate', 'repeat', 'palindrome', 'stutter',
    'interleave', 'transpose', 'euclid',
    'play', 'sequence', 'saw', 'pulse', 'sine', 'triangle', 'var', 'linvar', 'Clock', 'Scale', 'Root', 'PGroup',
    'Player', 'PlayerPattern', 'MouseX', 'MouseY', '__build_class__'
}
_COMPAT_WAVES = {'saw', 'pulse', 'sine', 'triangle'}
_COMPAT_WAVE_KEYWORDS = {
    'notes', 'dur', 'oct', 'sus', 'scale', 'root', 'amp', 'pan', 'cutoff', 'echo', 'decay',
    'delay', 'delaytime', 'echomix', 'feedback', 'velocity', 'rate', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'strum'
}
_COMPAT_SEQUENCE_KEYWORDS = {
    'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'synth',
    'amp', 'pan', 'cutoff', 'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'root', 'strum'
}
_COMPAT_PLAY_KEYWORDS = {'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'strum'}
_COMPAT_SCALE_NAMES = {
    'chromatic', 'major', 'minor', 'natural_minor', 'aeolian', 'dorian',
    'dorian2', 'mixolydian', 'major_pentatonic', 'majorPentatonic',
    'minor_pentatonic', 'minorPentatonic', 'melodic_minor', 'melodicMinor',
    'melodic_major', 'melodicMajor', 'harmonic_minor', 'harmonicMinor',
    'harmonic_major', 'harmonicMajor', 'diminished', 'egyptian', 'yu', 'zhi',
    'phrygian', 'prometheus', 'indian', 'locrian', 'locrian_major',
    'locrianMajor', 'lydian', 'lydian_minor', 'lydianMinor', 'custom',
    'hungarian_minor', 'hungarianMinor', 'romanian_minor', 'romanianMinor',
    'chinese', 'whole_tone', 'wholeTone', 'half_whole', 'halfWhole',
    'whole_half', 'wholeHalf', 'bebop_maj', 'bebopMaj', 'bebop_dorian',
    'bebopDorian', 'bebop_dom', 'bebopDom', 'bebop_mel_min', 'bebopMelMin',
    'blues', 'min_maj', 'minMaj', 'susb9', 'lydian_aug', 'lydianAug',
    'lydian_dom', 'lydianDom', 'mel_min5th', 'melMin5th', 'half_dim',
    'halfDim', 'altered'
}

class _DexterFoxDotCompatibilityVisitor(ast.NodeVisitor):
    def __init__(self, tree):
        self.diagnostics = []
        self._seen = set()
        self.defined = set()
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                self.defined.add(node.name)
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    for arg in node.args.posonlyargs + node.args.args + node.args.kwonlyargs:
                        self.defined.add(arg.arg)
                    if node.args.vararg:
                        self.defined.add(node.args.vararg.arg)
                    if node.args.kwarg:
                        self.defined.add(node.args.kwarg.arg)
            elif isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store):
                self.defined.add(node.id)

    def add(self, line, message):
        item = (int(line or 1), message)
        if item not in self._seen:
            self._seen.add(item)
            self.diagnostics.append(item)

    def visit_Import(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_ImportFrom(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_Call(self, node):
        name = node.func.id if isinstance(node.func, ast.Name) else None
        if name and name not in _COMPAT_CALLS and name not in self.defined:
            self.add(node.lineno, "Call '" + name + "()' is not in the embedded API (often an unsupported FoxDot synth or pattern helper).")
        keyword_names = None
        if name in _COMPAT_WAVES:
            keyword_names = _COMPAT_WAVE_KEYWORDS
        elif name == 'sequence':
            keyword_names = _COMPAT_SEQUENCE_KEYWORDS
        elif name == 'play':
            keyword_names = _COMPAT_PLAY_KEYWORDS
        if keyword_names is not None:
            for keyword in node.keywords:
                if keyword.arg is None:
                    self.add(node.lineno, 'Dynamic keyword expansion cannot be checked; use explicit supported controls.')
                elif keyword.arg not in keyword_names:
                    self.add(keyword.lineno or node.lineno, "Control '" + keyword.arg + "' is not supported by " + name + '() in this build.')
        if name in _COMPAT_WAVES.union({'play'}) and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            self.add(node.lineno, 'Sample-string playback is not included yet; sample support is deferred.')
        self.generic_visit(node)

    def visit_Attribute(self, node):
        if isinstance(node.value, ast.Name) and node.value.id == 'Clock' and node.attr not in {'bpm', 'meter'}:
            self.add(node.lineno, "Clock." + node.attr + ' is not supported; only Clock.bpm and fixed 4/4 Clock.meter are available.')
        if isinstance(node.value, ast.Name) and node.value.id == 'Scale' and node.attr not in _COMPAT_SCALE_NAMES.union({'default'}):
            self.add(node.lineno, 'Scale.' + node.attr + ' is not available in the embedded scale set.')
        if isinstance(node.value, ast.Attribute) and isinstance(node.value.value, ast.Name) and node.value.value.id == 'Scale' and node.value.attr == 'default' and node.attr != 'set':
            self.add(node.lineno, 'Scale.default only supports .set(...) and direct assignment in this build.')
        if isinstance(node.value, ast.Name) and node.value.id == 'Root' and node.attr != 'default':
            self.add(node.lineno, 'Root only supports Root.default in this build.')
        if isinstance(node.value, ast.Attribute) and isinstance(node.value.value, ast.Name) and node.value.value.id == 'Root' and node.value.attr == 'default' and node.attr != 'set':
            self.add(node.lineno, 'Root.default only supports .set(note).')
        if isinstance(node.value, ast.Name) and re.fullmatch(r'p[0-9]+', node.value.id):
            if not re.fullmatch(r'p[1-8]', node.value.id):
                self.add(node.lineno, 'Use one of the supported player names p1 through p8.')
            elif node.attr not in {'stop', 'mute', 'unmute'}:
                self.add(node.lineno, "Player " + node.value.id + ' supports .stop(), .mute(), and .unmute() in this build.')
        self.generic_visit(node)

    def visit_Subscript(self, node):
        if isinstance(node.value, ast.Name) and node.value.id == 'P':
            if isinstance(node.slice, ast.Slice):
                def integer_literal(bound):
                    if bound is None:
                        return True, None
                    if isinstance(bound, ast.Constant) and isinstance(bound.value, int):
                        return True, bound.value
                    if (isinstance(bound, ast.UnaryOp) and
                            isinstance(bound.op, (ast.USub, ast.UAdd)) and
                            isinstance(bound.operand, ast.Constant) and
                            isinstance(bound.operand.value, int)):
                        value = bound.operand.value
                        return True, -value if isinstance(bound.op, ast.USub) else value
                    return False, None
                bounds = (node.slice.lower, node.slice.upper, node.slice.step)
                parsed = [integer_literal(bound) for bound in bounds]
                if node.slice.upper is None:
                    self.add(node.lineno, 'P range slices require a finite stop value.')
                elif not all(value[0] for value in parsed):
                    self.add(node.lineno, 'P range slices require integer-literal bounds and step values.')
                else:
                    start = 0 if parsed[0][1] is None else parsed[0][1]
                    stop = parsed[1][1]
                    step = 1 if parsed[2][1] is None else parsed[2][1]
                    if step == 0:
                        self.add(node.lineno, 'P range slice step cannot be zero.')
                    elif len(range(start, stop, step)) > 256:
                        self.add(node.lineno, 'P range slices are limited to 256 generated items.')
            elif any(isinstance(item, ast.Constant) and isinstance(item.value, str) for item in ast.walk(node.slice)):
                self.add(node.lineno, 'String sample patterns are not included yet; sample support is deferred.')
        self.generic_visit(node)

    def visit_BinOp(self, node):
        if isinstance(node.op, (ast.BitOr, ast.BitAnd)) and any(
                isinstance(item, ast.Constant) and isinstance(item.value, str)
                for item in ast.walk(node)):
            self.add(node.lineno, 'Pattern | and & do not accept sample strings; use numeric pitches, rests, and chords.')
        if isinstance(node.op, ast.RShift) and isinstance(node.left, ast.Name) and isinstance(node.right, ast.Call):
            player = node.left.id
            if not re.fullmatch(r'p[1-8]', player):
                self.add(node.lineno, 'Use one of the supported player names p1 through p8; aliases and extra players are not available yet.')
        self.generic_visit(node)

def _dexter_run_compatibility_check():
    try:
        with open(_compat_source_path, 'r', encoding='utf-8') as source_file:
            source_text = source_file.read()
        try:
            tree = ast.parse(source_text, filename='<FoxDot script>')
        except SyntaxError as exc:
            line = exc.lineno or 1
            column = exc.offset or 1
            diagnostics = [(line, 'Python syntax error at column ' + str(column) + ': ' + (exc.msg or 'invalid syntax'))]
            tree = None
        if tree is not None:
            visitor = _DexterFoxDotCompatibilityVisitor(tree)
            visitor.visit(tree)
            diagnostics = sorted(visitor.diagnostics)
        if not diagnostics:
            if source_text.strip():
                report_text = ('No unsupported constructs detected in the embedded API subset.\n'
                               'Static check only: the script was not executed; runtime values can still fail.')
            else:
                report_text = 'The editor is empty. Static check only: no code was executed.'
        else:
            shown = diagnostics[:20]
            lines = ['Compatibility check found ' + str(len(diagnostics)) + ' item(s); the script was not executed:']
            lines.extend('Line ' + str(line) + ': ' + message for line, message in shown)
            if len(diagnostics) > len(shown):
                lines.append('...and ' + str(len(diagnostics) - len(shown)) + ' more item(s).')
            lines.append('Supported synth calls: sine, triangle, saw, pulse. Samples and the wider FoxDot synth/effect library are deferred.')
            report_text = '\n'.join(lines)
    except BaseException as exc:
        report_text = 'Compatibility check could not complete: ' + str(exc)
    with open(_compat_report_path, 'w', encoding='utf-8') as report_file:
        report_file.write(report_text)

_dexter_run_compatibility_check()
)PY";
    std::ostringstream script;
    script << "_compat_source_path = " << pythonLiteral(sourcePath) << "\n"
           << "_compat_report_path = " << pythonLiteral(reportPath) << "\n"
           << "exec(compile(" << pythonLiteral(compatibilityChecker)
           << ", '<Dexter compatibility checker>', 'exec'), globals(), globals())\n";
    return script.str();
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
    'chromatic': tuple(range(12)),
    'major': (0, 2, 4, 5, 7, 9, 11),
    'minor': (0, 2, 3, 5, 7, 8, 10),
    'natural_minor': (0, 2, 3, 5, 7, 8, 10),
    'aeolian': (0, 2, 3, 5, 7, 8, 10),
    'dorian': (0, 2, 3, 5, 7, 9, 10),
    'dorian2': (0, 1, 3, 5, 6, 8, 9, 11),
    'mixolydian': (0, 2, 4, 5, 7, 9, 10),
    'major_pentatonic': (0, 2, 4, 7, 9),
    'minor_pentatonic': (0, 3, 5, 7, 10),
    'melodic_minor': (0, 2, 3, 5, 7, 9, 11),
    'melodic_major': (0, 2, 4, 5, 7, 8, 11),
    'harmonic_minor': (0, 2, 3, 5, 7, 8, 11),
    'harmonic_major': (0, 2, 4, 5, 7, 8, 11),
    'diminished': (0, 1, 3, 4, 6, 7, 9, 10),
    'egyptian': (0, 2, 5, 7, 10),
    'yu': (0, 3, 5, 7, 10),
    'zhi': (0, 2, 5, 7, 9),
    'phrygian': (0, 1, 3, 5, 7, 8, 10),
    'prometheus': (0, 2, 4, 6, 11),
    'indian': (0, 4, 5, 7, 10),
    'locrian': (0, 1, 3, 5, 6, 8, 10),
    'locrian_major': (0, 2, 4, 5, 6, 8, 10),
    'lydian': (0, 2, 4, 6, 7, 9, 11),
    'lydian_minor': (0, 2, 4, 6, 7, 8, 10),
    'custom': (0, 2, 3, 5, 6, 9, 10),
    'hungarian_minor': (0, 2, 3, 6, 7, 8, 11),
    'romanian_minor': (0, 2, 3, 6, 7, 9, 10),
    'chinese': (0, 4, 6, 7, 11),
    'whole_tone': (0, 2, 4, 6, 8, 10),
    'half_whole': (0, 1, 3, 4, 6, 7, 9, 10),
    'whole_half': (0, 2, 3, 5, 6, 8, 9, 11),
    'bebop_maj': (0, 2, 4, 5, 7, 8, 9, 11),
    'bebop_dorian': (0, 2, 3, 4, 5, 9, 10),
    'bebop_dom': (0, 2, 4, 5, 7, 9, 10, 11),
    'bebop_mel_min': (0, 2, 3, 5, 7, 8, 9, 11),
    'blues': (0, 3, 5, 6, 7, 10),
    'min_maj': (0, 2, 3, 5, 7, 9, 11),
    'susb9': (0, 1, 3, 5, 7, 9, 10),
    'lydian_aug': (0, 2, 4, 6, 8, 9, 11),
    'lydian_dom': (0, 2, 4, 6, 7, 9, 10),
    'mel_min5th': (0, 2, 4, 5, 7, 8, 10),
    'half_dim': (0, 2, 3, 5, 6, 8, 10),
    'altered': (0, 1, 3, 4, 6, 8, 10),
}

_SCALE_ATTRIBUTES = {
    'majorPentatonic': 'major_pentatonic',
    'minorPentatonic': 'minor_pentatonic',
    'melodicMinor': 'melodic_minor',
    'melodicMajor': 'melodic_major',
    'harmonicMinor': 'harmonic_minor',
    'harmonicMajor': 'harmonic_major',
    'locrianMajor': 'locrian_major',
    'lydianMinor': 'lydian_minor',
    'hungarianMinor': 'hungarian_minor',
    'romanianMinor': 'romanian_minor',
    'wholeTone': 'whole_tone',
    'halfWhole': 'half_whole',
    'wholeHalf': 'whole_half',
    'bebopMaj': 'bebop_maj',
    'bebopDorian': 'bebop_dorian',
    'bebopDom': 'bebop_dom',
    'bebopMelMin': 'bebop_mel_min',
    'minMaj': 'min_maj',
    'lydianAug': 'lydian_aug',
    'lydianDom': 'lydian_dom',
    'melMin5th': 'mel_min5th',
    'halfDim': 'half_dim',
}

def _normalize_scale_name(name):
    name = str(name).strip().lower().replace(' ', '_').replace('-', '_')
    compact = name.replace('_', '')
    for attribute, canonical in _SCALE_ATTRIBUTES.items():
        if attribute.lower() == compact:
            return canonical
    return name

def _scale_intervals(name):
    if isinstance(name, (list, tuple)):
        values = []
        for value in name:
            if isinstance(value, bool):
                raise ValueError('custom scale intervals must be whole semitones from 0 to 11')
            try:
                interval = int(value)
            except (TypeError, ValueError, OverflowError):
                raise ValueError('custom scale intervals must be whole semitones from 0 to 11')
            if interval != value or interval < 0 or interval > 11:
                raise ValueError('custom scale intervals must be whole semitones from 0 to 11')
            values.append(interval)
        if not values or len(values) > 12 or values[0] != 0 or values != sorted(set(values)):
            raise ValueError('custom scales must start at 0 and contain 1..12 ascending unique intervals')
        return tuple(values)
    name = _normalize_scale_name(name)
    if name not in _SCALES:
        raise ValueError('unknown scale; use a supported Scale name or custom ascending semitone intervals')
    return _SCALES[name]

_ROOT_NOTES = {
    'c': 0, 'b#': 0, 'c#': 1, 'db': 1, 'd': 2, 'd#': 3, 'eb': 3,
    'e': 4, 'fb': 4, 'e#': 5, 'f': 5, 'f#': 6, 'gb': 6, 'g': 7,
    'g#': 8, 'ab': 8, 'a': 9, 'a#': 10, 'bb': 10, 'b': 11, 'cb': 11
}

def _root_midi(value):
    if isinstance(value, str):
        name = value.strip().lower().replace('♯', '#').replace('♭', 'b')
        if name not in _ROOT_NOTES:
            raise ValueError('root must be a pitch class from 0 to 11 or a note name such as C# or Eb')
        return 60 + _ROOT_NOTES[name]
    try:
        root = int(value)
    except (TypeError, ValueError, OverflowError):
        raise ValueError('root must be a pitch class from 0 to 11 or a MIDI note from 0 to 127')
    if root != value or not 0 <= root <= 127:
        raise ValueError('root must be a pitch class from 0 to 11 or a MIDI note from 0 to 127')
    return 60 + root if root <= 11 else root

def scale(name='major', root=60):
    global _dexter_scale, _dexter_root
    root = int(root)
    if not 0 <= root <= 127:
        raise ValueError('scale root must be a MIDI note from 0 to 127')
    _dexter_scale = _scale_intervals(name)
    _dexter_root = root

class _FoxDotScaleDefault:
    def set(self, name, root=None):
        scale(name, _dexter_root if root is None else _root_midi(root))

class _FoxDotScaleNamespace:
    def __init__(self):
        self.default = _FoxDotScaleDefault()
        for name in _SCALES:
            self.__dict__[name] = name
        for attribute, canonical in _SCALE_ATTRIBUTES.items():
            self.__dict__[attribute] = canonical
    def __setattr__(self, name, value):
        if name == 'default' and name in self.__dict__:
            self.__dict__['default'].set(value)
        else:
            self.__dict__[name] = value

Scale = _FoxDotScaleNamespace()

class _FoxDotRootDefault:
    def set(self, value):
        global _dexter_root
        _dexter_root = _root_midi(value)

class _FoxDotRootNamespace:
    def __init__(self):
        self.default = _FoxDotRootDefault()

Root = _FoxDotRootNamespace()

class _FoxDotFinitePattern(list):
    def _gcd(self, a, b):
        while b:
            a, b = b, a % b
        return a

    def _laced_length(self, left, right):
        return len(left) * len(right) // self._gcd(len(left), len(right))

    def _apply(self, left, right, operation):
        if left is None or right is None:
            return None
        left_group = isinstance(left, (list, tuple))
        right_group = isinstance(right, (list, tuple))
        if left_group and right_group:
            if len(left) != len(right):
                raise ValueError('chord groups must have matching sizes for P arithmetic')
            result = [self._apply(a, b, operation) for a, b in zip(left, right)]
            return tuple(result) if isinstance(left, tuple) else result
        if left_group:
            result = [self._apply(item, right, operation) for item in left]
            return tuple(result) if isinstance(left, tuple) else result
        if right_group:
            result = [self._apply(left, item, operation) for item in right]
            return tuple(result) if isinstance(right, tuple) else result
        if not isinstance(left, (int, float)) or not isinstance(right, (int, float)):
            raise TypeError('P arithmetic supports numeric values, rests, and numeric chords')
        try:
            value = operation(left, right)
        except ZeroDivisionError:
            value = 0
        if not isinstance(value, (int, float)):
            raise TypeError('P arithmetic must produce real numeric values')
        if value != value or value in (float('inf'), float('-inf')):
            raise ValueError('P arithmetic must produce finite values')
        return value

    def _binary(self, other, operation, reflected=False):
        if not self:
            return _FoxDotFinitePattern()
        if isinstance(other, (list, tuple)):
            if not other:
                return _FoxDotFinitePattern()
            length = self._laced_length(self, other)
            if length > 256:
                raise ValueError('P arithmetic is limited to 256 expanded items')
            values = []
            for index in range(length):
                left, right = self[index % len(self)], other[index % len(other)]
                values.append(self._apply(right, left, operation) if reflected else
                              self._apply(left, right, operation))
            return _FoxDotFinitePattern(values)
        if not isinstance(other, (int, float)):
            raise TypeError('P arithmetic supports a number or another finite pattern')
        values = [self._apply(other, item, operation) if reflected else
                  self._apply(item, other, operation) for item in self]
        return _FoxDotFinitePattern(values)

    def __add__(self, other):
        return self._binary(other, lambda a, b: a + b)

    def __radd__(self, other):
        return self._binary(other, lambda a, b: a + b, True)

    def __sub__(self, other):
        return self._binary(other, lambda a, b: a - b)

    def __rsub__(self, other):
        return self._binary(other, lambda a, b: a - b, True)

    def __mul__(self, other):
        return self._binary(other, lambda a, b: a * b)

    def __rmul__(self, other):
        return self._binary(other, lambda a, b: a * b, True)

    def __truediv__(self, other):
        return self._binary(other, lambda a, b: a / b)

    def __rtruediv__(self, other):
        return self._binary(other, lambda a, b: a / b, True)

    def __floordiv__(self, other):
        return self._binary(other, lambda a, b: a // b)

    def __rfloordiv__(self, other):
        return self._binary(other, lambda a, b: a // b, True)

    def __mod__(self, other):
        return self._binary(other, lambda a, b: a % b)

    def __rmod__(self, other):
        return self._binary(other, lambda a, b: a % b, True)

    def _power(self, base, exponent):
        if abs(base) > 256 or abs(exponent) > 16:
            raise ValueError('P exponents are limited to bases within 256 and exponents within 16')
        return base ** exponent

    def __pow__(self, other):
        return self._binary(other, self._power)

    def __rpow__(self, other):
        return self._binary(other, self._power, True)

    def __xor__(self, other):
        return self.__pow__(other)

    def __rxor__(self, other):
        return self.__rpow__(other)

    def __getitem__(self, key):
        value = list.__getitem__(self, key)
        return _FoxDotFinitePattern(value) if isinstance(key, slice) else value

    def reverse(self):
        return _FoxDotFinitePattern(list(reversed(self)))

    def rotate(self, amount=1):
        values = list(self)
        if not values:
            return _FoxDotFinitePattern()
        amount = int(amount) % len(values)
        return _FoxDotFinitePattern(values[amount:] + values[:amount])

    def repeat(self, count=2):
        count = int(count)
        if count < 1 or len(self) * count > 256:
            raise ValueError('repeat is limited to 256 expanded items and requires a positive count')
        return _FoxDotFinitePattern(list(self) * count)

    def stutter(self, count=2):
        counts = list(count) if isinstance(count, (list, tuple)) else [count]
        if not counts:
            raise ValueError('stutter requires at least one repeat count')
        if not self:
            return _FoxDotFinitePattern()
        length = self._laced_length(self, counts)
        if length > 256:
            raise ValueError('stutter is limited to 256 expanded items')
        values = []
        for index in range(length):
            repetitions = counts[index % len(counts)]
            if not isinstance(repetitions, (int, float)):
                raise ValueError('stutter counts must be whole numbers from 0 to 16')
            repeat_count = int(repetitions)
            if repeat_count != repetitions or repeat_count < 0 or repeat_count > 16:
                raise ValueError('stutter counts must be whole numbers from 0 to 16')
            if len(values) + repeat_count > 256:
                raise ValueError('stutter is limited to 256 expanded items')
            values.extend([self[index % len(self)]] * repeat_count)
        return _FoxDotFinitePattern(values)

    def palindrome(self):
        if len(self) * 2 > 256:
            raise ValueError('palindrome is limited to 256 expanded items')
        return _FoxDotFinitePattern(list(self) + list(reversed(self)))

    def _composition_items(self, value):
        items = list(value) if isinstance(value, (list, tuple)) else [value]
        if any(self._contains_string(item) for item in items):
            raise TypeError('P pattern composition supports numeric notes, rests, and chords; sample strings are deferred')
        return items

    def _contains_string(self, item):
        if isinstance(item, str):
            return True
        if isinstance(item, (list, tuple)):
            return any(self._contains_string(value) for value in item)
        return False

    def _append_chord_notes(self, item, notes):
        if item is None:
            return
        if isinstance(item, (list, tuple)):
            for value in item:
                self._append_chord_notes(value, notes)
            return
        notes.append(item)

    def _zip_item(self, left, right):
        notes = []
        self._append_chord_notes(left, notes)
        self._append_chord_notes(right, notes)
        if len(notes) > 8:
            raise ValueError('zipped chords can contain at most 8 notes')
        if not notes:
            return None
        return notes[0] if len(notes) == 1 else tuple(notes)

    def _zip_with(self, other, reflected=False):
        left = self._composition_items(other) if reflected else self._composition_items(self)
        right = self._composition_items(self) if reflected else self._composition_items(other)
        if not left or not right:
            return _FoxDotFinitePattern()
        length = self._laced_length(left, right)
        if length > 256:
            raise ValueError('P pattern zipping is limited to 256 expanded items')
        return _FoxDotFinitePattern([
            self._zip_item(left[index % len(left)], right[index % len(right)])
            for index in range(length)
        ])

    def __or__(self, other):
        left = self._composition_items(self)
        right = self._composition_items(other)
        if len(left) + len(right) > 256:
            raise ValueError('P pattern concatenation is limited to 256 items')
        return _FoxDotFinitePattern(left + right)

    def __ror__(self, other):
        left = self._composition_items(other)
        right = self._composition_items(self)
        if len(left) + len(right) > 256:
            raise ValueError('P pattern concatenation is limited to 256 items')
        return _FoxDotFinitePattern(left + right)

    def __and__(self, other):
        return self._zip_with(other)

    def __rand__(self, other):
        return self._zip_with(other, True)

class _FoxDotPatternLiteral:
    def __getitem__(self, items):
        if isinstance(items, slice):
            if items.stop is None:
                raise ValueError('P range slices require a finite stop value')
            for bound in (items.start, items.stop, items.step):
                if bound is not None and not isinstance(bound, int):
                    raise ValueError('P range slices require integer bounds and step values')
            start = 0 if items.start is None else items.start
            step = 1 if items.step is None else items.step
            if step == 0:
                raise ValueError('P range slice step cannot be zero')
            values = range(start, items.stop, step)
            if len(values) > 256:
                raise ValueError('P range slices are limited to 256 generated items')
            return _FoxDotFinitePattern(list(values))
        values = list(items) if isinstance(items, tuple) else [items]
        if len(values) > 256:
            raise ValueError('P[...] is limited to 256 literal items')
        return _FoxDotFinitePattern(values)

class _FoxDotChord(tuple):
    def __new__(cls, *notes):
        if len(notes) == 1 and isinstance(notes[0], (list, tuple)):
            notes = tuple(notes[0])
        if not 1 <= len(notes) <= 8:
            raise ValueError('a PGroup chord must contain 1..8 notes')
        for note in notes:
            if isinstance(note, bool) or not isinstance(note, (int, float)) or note != note:
                raise ValueError('PGroup chords accept numeric scale degrees only')
        return tuple.__new__(cls, notes)

def PGroup(*notes):
    return _FoxDotChord(*notes)

I = PGroup(0, 2, 4)
II = PGroup(1, 3, 5)
III = PGroup(2, 4, -1)
IV = PGroup(3, 5, 0)
V = PGroup(4, -1, 1)
VI = PGroup(5, 0, 2)
VII = PGroup(-1, 1, 3)
I7 = PGroup(0, 2, 4, 6)
II7 = PGroup(1, 3, 5, 7)
III7 = PGroup(2, 4, -1, 8)
IV7 = PGroup(3, 5, 0, 9)
V7 = PGroup(4, -1, 1, 10)
VI7 = PGroup(5, 0, 2, 11)
VII7 = PGroup(-1, 1, 3, 12)

P = _FoxDotPatternLiteral()

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

def palindrome(pattern):
    values = list(pattern)
    if len(values) <= 1:
        return values
    if len(values) * 2 - 2 > 256:
        raise ValueError('palindrome must produce no more than 256 steps')
    return values + values[-2:0:-1]

def stutter(pattern, count=2):
    values = list(pattern)
    count = int(count)
    if count < 1 or count > 16 or len(values) * count > 256:
        raise ValueError('stutter count must be 1 to 16 and produce no more than 256 steps')
    return [item for item in values for _ in range(count)]

def interleave(*patterns):
    if len(patterns) < 2:
        raise ValueError('interleave requires at least two patterns')
    values = [list(pattern) for pattern in patterns]
    total = sum(len(pattern) for pattern in values)
    if total > 256:
        raise ValueError('interleave must produce no more than 256 steps')
    result = []
    for index in range(max((len(pattern) for pattern in values), default=0)):
        for pattern in values:
            if index < len(pattern):
                result.append(pattern[index])
    return result

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

def play(notes, step=1.0, duration=None, velocity=100, offset=0.0, degrees=False,
         strum=0.0,
         _octave_control=None, _base_octave=5, _degrees_per_octave=7,
         _attack_control=None, _release_control=None,
         _attack_default=0.01, _release_default=0.04):
    global _dexter_length, _dexter_slots, _dexter_default_touched
    _dexter_default_touched = True
    if isinstance(notes, _FoxDotChord):
        notes = [notes]
    elif isinstance(notes, (int, float)):
        notes = [notes]
    if isinstance(notes, str):
        raise ValueError('FoxDot sample strings are not included yet; sample support is deferred')
    notes = list(notes)
    stepIsVar = isinstance(step, _DexterTimeVar)
    steps = None if stepIsVar else _timing_pattern(step, len(notes), 'dur')
    durationSource = step if duration is None else duration
    durationIsVar = isinstance(durationSource, _DexterTimeVar)
    durations = None if durationIsVar else _timing_pattern(durationSource, len(notes), 'sus')
    offset = float(offset)
    if offset != offset or offset < 0.0 or offset > 4096.0:
        raise ValueError('offset must be between 0 and 4096 beats')
    if stepIsVar and any(value < 0.1 or value > 4096.0 for value in step.values):
        raise ValueError('dur values must be between 0.1 and 4096 beats')
    if durationIsVar and any(value < 0.02 or value > 4096.0 for value in durationSource.values):
        raise ValueError('sus values must be between 0.02 and 4096 beats')
    if _octave_control is not None and any(value < 0.0 or value > 10.0 for value in _octave_control.values):
        raise ValueError('oct values must be from 0 to 10')
    if _attack_control is not None and any(value < 0.0 or value > 3.0 for value in _attack_control.values):
        raise ValueError('atk values must be from 0 to 3 seconds')
    if _release_control is not None and any(value < 0.0 or value > 3.0 for value in _release_control.values):
        raise ValueError('rel values must be from 0 to 3 seconds')
    strumIsVar = isinstance(strum, _DexterTimeVar)
    if strumIsVar:
        if any(value < 0.0 or value > 1.0 for value in strum.values):
            raise ValueError('strum values must be between 0 and 1 beat')
    else:
        strum = float(strum)
        if strum != strum or not 0.0 <= strum <= 1.0:
            raise ValueError('strum must be between 0 and 1 beat')
    velocityIsVar = isinstance(velocity, _DexterTimeVar)
    if velocityIsVar:
        if any(value < 1.0 or value > 127.0 for value in velocity.values):
            raise ValueError('velocity values must be from 1 to 127')
    else:
        velocity = int(velocity)
        if not 1 <= velocity <= 127:
            raise ValueError('velocity must be 1..127')
    if _dexter_slots + len(notes) > 256:
        raise ValueError('a pattern is limited to 256 step slots')
    starts = []
    stepValues = []
    durationValues = []
    start = offset
    for i in range(len(notes)):
        stepValue = step.at(start) if stepIsVar else steps[i]
        durationValue = durationSource.at(start) if durationIsVar else durations[i]
        if stepValue != stepValue or stepValue < 0.1 or stepValue > 4096.0:
            raise ValueError('dur values must be between 0.1 and 4096 beats')
        if durationValue != durationValue or durationValue < 0.02 or durationValue > stepValue + 1e-9:
            raise ValueError('each sus value must be at least 0.02 beats and fit within dur')
        starts.append(start)
        stepValues.append(stepValue)
        durationValues.append(durationValue)
        start += stepValue
    totalBeats = sum(stepValues)
    if offset + totalBeats > 4096.0:
        raise ValueError('offset and pattern length must fit within 4096 beats')
    additions = []
    for i, item in enumerate(notes):
        start = starts[i]
        noteVelocity = int(round(velocity.at(start))) if velocityIsVar else velocity
        if not 1 <= noteVelocity <= 127:
            raise ValueError('velocity values must be from 1 to 127')
        if item is None:
            continue
        chord = list(item) if isinstance(item, (list, tuple)) else [item]
        if len(chord) > 8:
            raise ValueError('a chord can contain at most 8 notes')
        strumValue = strum.at(start) if strumIsVar else strum
        for chordIndex, note in enumerate(chord):
            note = int(note)
            if _octave_control is not None:
                octave = int(round(_octave_control.at(start)))
                note += (octave - _base_octave) * _degrees_per_octave
            if degrees:
                octave, degree = divmod(note, len(_dexter_scale))
                note = _dexter_root + octave * 12 + _dexter_scale[degree]
            if not 0 <= note <= 127:
                raise ValueError('notes must resolve to MIDI pitches 0..127')
            attackValue = _attack_control.at(start) if _attack_control is not None else float(_attack_default)
            releaseValue = _release_control.at(start) if _release_control is not None else float(_release_default)
            if attackValue != attackValue or not 0.0 <= attackValue <= 3.0:
                raise ValueError('atk must be between 0 and 3 seconds')
            if releaseValue != releaseValue or not 0.0 <= releaseValue <= 3.0:
                raise ValueError('rel must be between 0 and 3 seconds')
            additions.append((start, durationValues[i], note, noteVelocity, attackValue, releaseValue, strumValue * chordIndex))
    if len(_dexter_events) + len(additions) > 256:
        raise ValueError('a pattern is limited to 256 note events')
    candidate = _dexter_events + additions
    points = sorted([(event[0] + event[6], 1, event[2]) for event in candidate] +
                    [(event[0] + event[6] + event[1], -1, event[2]) for event in candidate])
    active = 0
    active_pitches = set()
    for beat, delta, note in points:
        if delta < 0:
            active -= 1
            active_pitches.discard(note)
        else:
            if note in active_pitches:
                raise ValueError('MIDI pitch ' + str(note) + ' overlaps itself near beat ' + str(round(beat, 3)) + '; shorten sus or adjust strum/delay')
            if active >= 8:
                raise ValueError('pattern exceeds 8-note polyphony near beat ' + str(round(beat, 3)) + '; reduce chord size or shorten sus')
            active += 1
            active_pitches.add(note)
    _dexter_events.extend(additions)
    _dexter_slots += len(notes)
    _dexter_length = max(_dexter_length, offset + totalBeats)
class _DexterMouseControl:
    def __init__(self, axis):
        self.axis = axis

def MouseX():
    return _DexterMouseControl('x')

def MouseY():
    return _DexterMouseControl('y')

class _DexterTimeVar:
    def __init__(self, values, durations, linear=False):
        self.values = [float(value) for value in (values if isinstance(values, (list, tuple)) else [values])]
        self.durations = [float(value) for value in (durations if isinstance(durations, (list, tuple)) else [durations])]
        if not self.values or len(self.values) > 8:
            raise ValueError('var/linvar supports 1..8 values')
        if len(self.durations) == 1 and len(self.values) > 1:
            self.durations = self.durations * len(self.values)
        if len(self.durations) != len(self.values):
            raise ValueError('var/linvar durations must be one value or match the values')
        if any(value != value or value <= 0.0 or value > 4096.0 for value in self.durations):
            raise ValueError('var/linvar durations must be between 0 and 4096 beats')
        if any(value != value for value in self.values):
            raise ValueError('var/linvar values must be finite numbers')
        self.linear = bool(linear)

    def wire(self):
        mode = 'l' if self.linear else 'v'
        token = mode + ':' + ','.join(str(value) for value in self.values) + ':' + ','.join(str(value) for value in self.durations)
        if len(token) > 256:
            raise ValueError('var/linvar control data exceeds the supported size')
        return token

    def at(self, beat):
        cycle = sum(self.durations)
        position = max(0.0, float(beat)) % cycle
        for index, duration in enumerate(self.durations):
            if position < duration or index + 1 == len(self.durations):
                if not self.linear or len(self.values) == 1:
                    return self.values[index]
                progress = position / duration
                following = self.values[(index + 1) % len(self.values)]
                return self.values[index] + (following - self.values[index]) * progress
            position -= duration
        return self.values[-1]

def var(values, durations=4.0):
    return _DexterTimeVar(values, durations, False)

def linvar(values, durations=4.0):
    return _DexterTimeVar(values, durations, True)

def _control_value(value, name, lower, upper):
    if isinstance(value, _DexterTimeVar):
        if any(item < lower or item > upper for item in value.values):
            raise ValueError(name + ' time-var values are outside the supported range')
        return value.values[0], value.wire()
    result = float(value)
    if result != result or result < lower or result > upper:
        raise ValueError(name + ' is outside the supported range')
    return result, '-'

def _event_control(value, name, lower, upper):
    if isinstance(value, _DexterTimeVar):
        if any(item < lower or item > upper for item in value.values):
            raise ValueError(name + ' time-var values are outside the supported range')
        return value.values[0], value
    result = float(value)
    if result != result or result < lower or result > upper:
        raise ValueError(name + ' is outside the supported range')
    return result, None

class PlayerPattern:
    def __init__(self, notes, step=1.0, duration=None, velocity=100,
                 offset=0.0, degrees=False, synth='sine', amp=1.0, pan=0.0,
                 cutoff=20000.0, echo=0.0, delay=0.0, scaleName=None,
                 atk=0.01, rel=0.04, octaveControl=None, baseOctave=5, degreesPerOctave=7,
                 reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
        self.notes = notes
        self.step = step
        self.duration = duration
        self.velocity = velocity
        self.offset = offset
        self.degrees = degrees
        self.rootMidi = None if root is None else _root_midi(root)
        if synth not in ('sine', 'triangle', 'saw', 'square'):
            raise ValueError('synth must be sine, triangle, saw, or square')
        self.synth = synth
        self.scaleName = scaleName
        self.amp, self.ampControl = _control_value(amp, 'amp', 0.0, 1.0)
        self.mousePan = isinstance(pan, _DexterMouseControl)
        if self.mousePan:
            if pan.axis != 'x':
                raise ValueError('pan supports MouseX() only')
            self.pan, self.panControl = 0.0, '-'
        else:
            self.pan, self.panControl = _control_value(pan, 'pan', -1.0, 1.0)
        self.mouseCutoff = isinstance(cutoff, _DexterMouseControl)
        if self.mouseCutoff:
            if cutoff.axis != 'y':
                raise ValueError('cutoff supports MouseY() only')
            self.cutoff, self.cutoffControl = 4100.0, '-'
        else:
            self.cutoff, self.cutoffControl = _control_value(cutoff, 'cutoff', 20.0, 20000.0)
        self.echoBeats, self.echoControl = _control_value(echo, 'echo', 0.0, 64.0)
        self.delayBeats, self.delayControl = _event_control(delay, 'delay', 0.0, 8.0)
        delaytimeValue = 0.0 if delaytime is None else delaytime
        self.delayTimeSeconds, self.delayTimeControl = _control_value(delaytimeValue, 'delaytime', 0.0, 4.0)
        self.echoMix, self.echoMixControl = _control_value(echomix, 'echomix', 0.0, 1.0)
        self.feedback, self.feedbackControl = (-1.0, None) if feedback is None else _control_value(feedback, 'feedback', 0.0, 0.95)
        if reverb not in ('freeverb', 'freeverb2', 'gverb', 'none'):
            raise ValueError('reverb must be freeverb, freeverb2, gverb, or none')
        self.reverb = reverb
        self.reverbIndex = ('none', 'freeverb', 'freeverb2', 'gverb').index(reverb)
        self.room = float(room)
        self.mix = float(mix)
        self.damp = float(damp)
        self.revtime = float(revtime)
        self.spread = float(spread)
        if any(value != value for value in (self.room, self.mix, self.damp, self.revtime, self.spread)):
            raise ValueError('reverb controls must be finite numbers')
        if not 0.0 <= self.room <= 1.0 or not 0.0 <= self.mix <= 1.0 or not 0.0 <= self.damp <= 1.0:
            raise ValueError('room, mix, and damp must be between 0 and 1')
        if not 0.1 <= self.revtime <= 10.0:
            raise ValueError('revtime must be between 0.1 and 10 seconds')
        if not 0.0 <= self.spread <= 100.0:
            raise ValueError('spread must be between 0 and 100')
        self.attackSeconds, self.attackControl = _event_control(atk, 'atk', 0.0, 3.0)
        self.releaseSeconds, self.releaseControl = _event_control(rel, 'rel', 0.0, 3.0)
        self.lpfHz, self.lpfControl = _event_control(lpf, 'lpf', 0.0, 20000.0)
        self.lpr, self.lprControl = _event_control(lpr, 'lpr', 0.05, 1.0)
        self.hpfHz, self.hpfControl = _event_control(hpf, 'hpf', 0.0, 20000.0)
        self.hpr, self.hprControl = _event_control(hpr, 'hpr', 0.05, 1.0)
        self.bpfHz, self.bpfControl = _event_control(bpf, 'bpf', 0.0, 20000.0)
        self.bpr, self.bprControl = _event_control(bpr, 'bpr', 0.05, 1.0)
        self.echoDecayBeats, self.echoDecayControl = _event_control(decay, 'decay', 0.0, 64.0)
        self.strumBeats, self.strumControl = _event_control(strum, 'strum', 0.0, 1.0)
        self.octaveControl = octaveControl
        self.baseOctave = baseOctave
        self.degreesPerOctave = degreesPerOctave
        if self.attackSeconds != self.attackSeconds or not 0.0 <= self.attackSeconds <= 3.0:
            raise ValueError('atk must be between 0 and 3 seconds')
        if self.releaseSeconds != self.releaseSeconds or not 0.0 <= self.releaseSeconds <= 3.0:
            raise ValueError('rel must be between 0 and 3 seconds')

def sequence(notes, step=1.0, duration=None, velocity=100,
             offset=0.0, degrees=False, synth='sine', amp=1.0, pan=0.0,
             cutoff=20000.0, echo=0.0, delay=0.0, atk=0.01, rel=0.04,
             reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                   lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    if isinstance(notes, _FoxDotChord):
        notes = [notes]
    return PlayerPattern(notes, step, duration, velocity, offset, degrees,
                        synth, amp, pan, cutoff, echo, delay, atk=atk, rel=rel,
                        reverb=reverb, room=room, mix=mix, damp=damp,
                          revtime=revtime, spread=spread, lpf=lpf, lpr=lpr, hpf=hpf, hpr=hpr, bpf=bpf, bpr=bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)

def _fox_wave(synthName, notes=0, dur=1.0, oct=5, sus=None, scale=None,
              amp=1.0, pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0,
              velocity=100, rate=0.0, atk=0.01, rel=0.04,
              reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                   lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    octaveControl = oct if isinstance(oct, _DexterTimeVar) else None
    if octaveControl is not None:
        if any(value < 0.0 or value > 10.0 for value in octaveControl.values):
            raise ValueError('oct values must be from 0 to 10')
        baseOctave = int(round(octaveControl.values[0]))
    else:
        baseOctave = int(oct)
    if not 0 <= baseOctave <= 10:
        raise ValueError('oct must be from 0 to 10 in the embedded FoxDot subset')
    scaleName = None if scale is None else _scale_intervals(scale)
    degreesPerOctave = len(scaleName) if scaleName is not None else len(_dexter_scale)
    shift = (baseOctave - 5) * degreesPerOctave
    def shift_note(note):
        if note is None:
            return None
        if isinstance(note, _FoxDotChord):
            return _FoxDotChord(*[shift_note(value) for value in note])
        if isinstance(note, (list, tuple)): 
            return [shift_note(value) for value in note]
        return int(note) + shift
    if isinstance(notes, _FoxDotChord):
        notes = [notes]
    elif isinstance(notes, (int, float)):
        notes = [notes]
    if isinstance(notes, str):
        raise ValueError('FoxDot sample strings are not included yet; sample support is deferred')
    notes = [shift_note(note) for note in notes]
    return PlayerPattern(notes, dur, dur if sus is None else sus, velocity,
                         0.0, True, synthName, amp, pan, cutoff, echo, delay,
                         scaleName, atk, rel, octaveControl, baseOctave, degreesPerOctave,
                           reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)
def saw(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
        pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    return _fox_wave('saw', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)

def pulse(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
          pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    return _fox_wave('square', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)

def sine(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
         pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    return _fox_wave('sine', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)

def triangle(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
             pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0):
    return _fox_wave('triangle', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum)

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
            if pattern.rootMidi is not None:
                _dexter_root = pattern.rootMidi
            play(pattern.notes, step=pattern.step, duration=pattern.duration,
                 velocity=pattern.velocity, offset=pattern.offset,
                 degrees=pattern.degrees, _octave_control=pattern.octaveControl,
                 _base_octave=pattern.baseOctave,
                 _degrees_per_octave=pattern.degreesPerOctave,
                 _attack_control=pattern.attackControl,
                 _release_control=pattern.releaseControl,
                 _attack_default=pattern.attackSeconds,
                  _release_default=pattern.releaseSeconds,
                  strum=pattern.strumControl if pattern.strumControl is not None else pattern.strumBeats)
            compiledLength = _dexter_length
            compiledEvents = [event[:6] + (event[6] + (0.0 if pattern.delayControl is not None else pattern.delayBeats),) for event in _dexter_events]
        finally:
            _dexter_events, _dexter_length, _dexter_slots, _dexter_default_touched = saved[:4]
            _dexter_scale, _dexter_root = saved[4], saved[5]
        playerIndex = self.number - 1
        if not 0 <= playerIndex < 8:
            raise ValueError('player number must be 1..8')
        synthIndex = ('sine', 'triangle', 'saw', 'square').index(pattern.synth)
        _dexter_updates[playerIndex] = (False, _dexter_bpm, compiledLength,
                                        compiledEvents, synthIndex, pattern.amp,
                                        pattern.pan, pattern.cutoff, pattern.echoBeats,
                                        pattern.delayTimeSeconds, pattern.mousePan, pattern.mouseCutoff,
                                        pattern.attackSeconds, pattern.releaseSeconds, pattern.ampControl,
                                        pattern.panControl, pattern.cutoffControl,
                                        pattern.echoControl, pattern.delayTimeControl,
                                        pattern.reverbIndex, pattern.mix, pattern.room, pattern.damp,
                                        pattern.revtime, pattern.spread,
                                         pattern.lpfHz, pattern.lpr, pattern.hpfHz, pattern.hpr,
                                         pattern.bpfHz, pattern.bpr,
                                         pattern.lpfControl, pattern.lprControl,
                                         pattern.hpfControl, pattern.hprControl,
                                         pattern.bpfControl, pattern.bprControl, pattern.echoDecayBeats, pattern.echoDecayControl,
                                        pattern.echoMix, pattern.echoMixControl, pattern.feedback, pattern.feedbackControl, pattern.delayControl)
        if playerIndex == 0:
            _dexter_default_touched = False
        return self

    def stop(self):
        _dexter_updates[self.number - 1] = (True, _dexter_bpm, 1.0, [], 0, 1.0,
                                             0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-')

    def mute(self):
        _dexter_mutes[self.number - 1] = True
        return self

    def unmute(self):
        _dexter_mutes[self.number - 1] = False
        return self

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
           << "'_dexter_scale': (0, 2, 4, 5, 7, 9, 11), '_dexter_root': 60, '_dexter_mutes': {}})\n"
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
            << "            _dexter_scope['_dexter_updates'][0] = (False, _dexter_scope['_dexter_bpm'], _dexter_scope['_dexter_length'], list(_dexter_scope['_dexter_events']), 0, 1.0, 0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-')\n"
            << "        _updates = _dexter_scope['_dexter_updates']\n"
            << "        _mutes = _dexter_scope['_dexter_mutes']\n"
            << "        _f.write('OK\\t{}\\t{}\\n'.format(len(_updates), len(_mutes)))\n"
            << "        for _player in sorted(_updates):\n"
            << "            _stopped, _bpm, _length, _events, _synth, _amp, _pan, _cutoff, _echo, _delay, _mouse_pan, _mouse_cutoff, _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control, _reverb, _mix, _room, _damp, _revtime, _spread, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, _lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control, _decay, _decay_control, _echo_mix, _echo_mix_control, _feedback, _feedback_control, _note_delay_control = _updates[_player]\n"
            << "            _f.write('P\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, int(_stopped), _bpm, _length, len(_events), _synth, _amp, _pan, _cutoff, _echo, _delay, int(_mouse_pan), int(_mouse_cutoff), _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control))\n"
            << "            _f.write('R\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _reverb, _mix, _room, _damp, _revtime, _spread))\n"
             << "            _f.write('E\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _decay, _decay_control.wire() if _decay_control is not None else '-', _echo_mix, _echo_mix_control, _feedback, _feedback_control if _feedback_control is not None else '-', _note_delay_control.wire() if hasattr(_note_delay_control, 'wire') else (_note_delay_control if _note_delay_control is not None else '-')))\n"
              << "            _controls = (_lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control)\n"
             << "            _f.write('F\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, *[control.wire() if control is not None else '-' for control in _controls]))\n"
            << "            for _e in _events:\n"
            << "                _f.write('N\\t{}\t{}\t{}\t{}\t{}\t{}\\t{}\\n'.format(*_e))\n"
            << "        for _player, _muted in sorted(_mutes.items()):\n"
            << "            _f.write('M\\t{}\\t{}\\n'.format(_player, int(_muted)))\n"
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
    std::size_t muteCount = 0;
    if (!(fields >> marker >> updateCount >> muteCount) || marker != "OK" ||
        updateCount > 8 || muteCount > 8) {
        error = "Python returned an invalid player update header: " + header;
        return false;
    }
    pattern.players.clear();
    pattern.players.reserve(updateCount);
    pattern.mutes.clear();
    pattern.mutes.reserve(muteCount);
    std::array<bool, 8> seenPlayers{};
    for (std::size_t playerNumber = 0; playerNumber < updateCount; ++playerNumber) {
        char record = 0;
        int stopped = 0;
        std::size_t eventCount = 0;
        PythonPlayerUpdate update;
        int mousePan = 0;
        int mouseCutoff = 0;
        if (!(input >> record >> update.playerIndex >> stopped >> update.bpm >>
              update.lengthBeats >> eventCount >> update.synthIndex >> update.amplitude >>
              update.pan >> update.cutoffHz >> update.echoBeats >> update.delayTimeSeconds >>
              mousePan >> mouseCutoff >> update.attackSeconds >> update.releaseSeconds >>
              update.ampControlToken >> update.panControlToken >> update.cutoffControlToken >>
              update.echoControlToken >> update.delayTimeControlToken) ||
            record != 'P' || update.playerIndex < 0 || update.playerIndex >= 8 ||
            stopped < 0 || stopped > 1 ||
            seenPlayers[static_cast<std::size_t>(update.playerIndex)] || eventCount > 256 ||
            !std::isfinite(update.bpm) || update.bpm < 20.0 || update.bpm > 300.0 ||
            !std::isfinite(update.lengthBeats) || update.lengthBeats <= 0.0 ||
            update.lengthBeats > 4096.0 || update.synthIndex < 0 || update.synthIndex > 3 ||
            !std::isfinite(update.amplitude) || update.amplitude < 0.0 || update.amplitude > 1.0 ||
            !std::isfinite(update.pan) || update.pan < -1.0 || update.pan > 1.0 ||
            !std::isfinite(update.cutoffHz) || update.cutoffHz < 20.0 ||
            update.cutoffHz > 20000.0 || !std::isfinite(update.echoBeats) ||
            update.echoBeats < 0.0 || update.echoBeats > 64.0 ||
            !std::isfinite(update.delayTimeSeconds) || update.delayTimeSeconds < 0.0 ||
            update.delayTimeSeconds > 4.0 || mousePan < 0 || mousePan > 1 ||
            mouseCutoff < 0 || mouseCutoff > 1 ||
            !std::isfinite(update.attackSeconds) || update.attackSeconds < 0.0 ||
            update.attackSeconds > 3.0 || !std::isfinite(update.releaseSeconds) ||
            update.releaseSeconds < 0.0 || update.releaseSeconds > 3.0) {
            std::ostringstream detail;
            detail << "Python returned malformed player update data (record=" << record
                   << ", player=" << update.playerIndex + 1
                   << ", stopped=" << stopped << ", events=" << eventCount
                   << ", BPM=" << update.bpm << ", synth=" << update.synthIndex
                   << "). Check the copied diagnostic for the full exception.";
            error = detail.str();
            return false;
        }
        char effectRecord = 0;
        int effectPlayer = -1;
        if (!(input >> effectRecord >> effectPlayer >> update.reverbIndex >> update.reverbMix >>
              update.reverbRoom >> update.reverbDamp >> update.reverbTime >>
              update.reverbSpread) ||
            effectRecord != 'R' || effectPlayer != update.playerIndex ||
            update.reverbIndex < 0 || update.reverbIndex > 3 ||
            !std::isfinite(update.reverbMix) || update.reverbMix < 0.0 || update.reverbMix > 1.0 ||
            !std::isfinite(update.reverbRoom) || update.reverbRoom < 0.0 || update.reverbRoom > 1.0 ||
            !std::isfinite(update.reverbDamp) || update.reverbDamp < 0.0 || update.reverbDamp > 1.0 ||
            !std::isfinite(update.reverbTime) || update.reverbTime < 0.1 || update.reverbTime > 10.0 ||
            !std::isfinite(update.reverbSpread) || update.reverbSpread < 0.0 || update.reverbSpread > 100.0) {
            error = "Python returned malformed reverb controls.";
            return false;
        }
        char decayRecord = 0;
        int decayPlayer = -1;
        if (!(input >> decayRecord >> decayPlayer >> update.echoDecayBeats >> update.echoDecayControlToken >>
              update.echoMix >> update.echoMixControlToken >> update.echoFeedback >> update.echoFeedbackControlToken >> update.noteDelayControlToken) ||
            decayRecord != 'E' || decayPlayer != update.playerIndex ||
            !std::isfinite(update.echoDecayBeats) || update.echoDecayBeats < 0.0 || update.echoDecayBeats > 64.0 ||
            !std::isfinite(update.echoMix) || update.echoMix < 0.0 || update.echoMix > 1.0 ||
            !std::isfinite(update.echoFeedback) ||
            (update.echoFeedback < 0.0 && update.echoFeedback != -1.0) || update.echoFeedback > 0.95) {
            error = "Python returned malformed FoxDot echo and plugin echo controls.";
            return false;
        }
        char filterRecord = 0;
        int filterPlayer = -1;
        if (!(input >> filterRecord >> filterPlayer >> update.lpfHz >> update.lpr >>
              update.hpfHz >> update.hpr >> update.bpfHz >> update.bpr >>
              update.lpfControlToken >> update.lprControlToken >>
              update.hpfControlToken >> update.hprControlToken >>
              update.bpfControlToken >> update.bprControlToken) ||
            filterRecord != 'F' || filterPlayer != update.playerIndex ||
            !std::isfinite(update.lpfHz) || update.lpfHz < 0.0 || update.lpfHz > 20000.0 ||
            !std::isfinite(update.lpr) || update.lpr < 0.05 || update.lpr > 1.0 ||
            !std::isfinite(update.hpfHz) || update.hpfHz < 0.0 || update.hpfHz > 20000.0 ||
            !std::isfinite(update.hpr) || update.hpr < 0.05 || update.hpr > 1.0 ||
            !std::isfinite(update.bpfHz) || update.bpfHz < 0.0 || update.bpfHz > 20000.0 ||
            !std::isfinite(update.bpr) || update.bpr < 0.05 || update.bpr > 1.0) {
            error = "Python returned malformed filter controls.";
            return false;
        }
        seenPlayers[static_cast<std::size_t>(update.playerIndex)] = true;
        update.stopped = stopped != 0;
        update.mousePan = mousePan != 0;
        update.mouseCutoff = mouseCutoff != 0;
        update.events.reserve(eventCount);
        for (std::size_t i = 0; i < eventCount; ++i) {
            char noteRecord = 0;
            PythonNoteEvent event;
            if (!(input >> noteRecord >> event.beat >> event.durationBeats >>
                  event.midiNote >> event.velocity >> event.attackSeconds >> event.releaseSeconds >> event.delayBeats) ||
                noteRecord != 'N' ||
                !std::isfinite(event.beat) || !std::isfinite(event.durationBeats) ||
                !std::isfinite(event.delayBeats) || event.delayBeats < 0.0 || event.delayBeats > 15.0 ||
                event.beat < 0.0 || event.beat > update.lengthBeats ||
                event.durationBeats < 0.02 ||
                event.beat + event.durationBeats > update.lengthBeats + 1e-9 ||
                event.midiNote < 0 || event.midiNote > 127 ||
                event.velocity < 1 || event.velocity > 127 ||
                !std::isfinite(event.attackSeconds) || event.attackSeconds < 0.0 ||
                event.attackSeconds > 3.0 || !std::isfinite(event.releaseSeconds) ||
                event.releaseSeconds < 0.0 || event.releaseSeconds > 3.0) {
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
        boundaries.reserve(update.events.size() * 4);
        for (const auto& event : update.events) {
            double startBeat = std::fmod(event.beat + event.delayBeats, update.lengthBeats);
            if (startBeat < 0.0) startBeat += update.lengthBeats;
            const double endBeat = startBeat + event.durationBeats;
            if (endBeat <= update.lengthBeats) {
                boundaries.push_back({startBeat, event.midiNote, 1});
                boundaries.push_back({endBeat, event.midiNote, -1});
            } else {
                boundaries.push_back({0.0, event.midiNote, 1});
                boundaries.push_back({endBeat - update.lengthBeats, event.midiNote, -1});
                boundaries.push_back({startBeat, event.midiNote, 1});
                boundaries.push_back({update.lengthBeats, event.midiNote, -1});
            }
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
                if (activePitches[pitch]) {
                    std::ostringstream message;
                    message << "Player " << (update.playerIndex + 1)
                            << " overlaps MIDI pitch " << boundary.note
                            << " near beat " << boundary.beat
                            << "; shorten sus or adjust strum/delay.";
                    error = message.str();
                    return false;
                }
                if (activeNotes >= 8) {
                    std::ostringstream message;
                    message << "Player " << (update.playerIndex + 1)
                            << " exceeds 8-note polyphony near beat " << boundary.beat
                            << "; reduce chord size or shorten sus.";
                    error = message.str();
                    return false;
                }
                ++activeNotes;
                activePitches[pitch] = true;
            }
        }
        pattern.players.push_back(std::move(update));
    }
    std::array<bool, 8> seenMutes{};
    for (std::size_t i = 0; i < muteCount; ++i) {
        char record = 0;
        int playerIndex = -1;
        int muted = -1;
        if (!(input >> record >> playerIndex >> muted) || record != 'M' ||
            playerIndex < 0 || playerIndex >= 8 || muted < 0 || muted > 1 ||
            seenMutes[static_cast<std::size_t>(playerIndex)]) {
            error = "Python returned an invalid player mute control (record, player index, or mute state).";
            return false;
        }
        seenMutes[static_cast<std::size_t>(playerIndex)] = true;
        pattern.mutes.push_back(PythonPlayerMute{playerIndex, muted != 0});
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

bool PortablePython::checkCompatibility(const std::filesystem::path& runtimeDirectory,
                                       const std::string& source,
                                       std::string& report,
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
                      (L"dexter-check-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(number));
    const auto sourcePath = base.wstring() + L".py";
    const auto reportPath = base.wstring() + L".txt";
    {
        std::ofstream sourceFile(sourcePath, std::ios::binary);
        if (!sourceFile) {
            error = "Could not create a temporary Python source file for compatibility checking.";
            return false;
        }
        sourceFile.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    const auto script = makeCompatibilityScript(pathAsUtf8(sourcePath),
                                                pathAsUtf8(reportPath));
    int result = -1;
    {
        std::lock_guard<std::mutex> lock(gPythonMutex);
        const int gil = gPython.gilStateEnsure();
        result = gPython.runSimpleStringFlags(script.c_str(), nullptr);
        gPython.gilStateRelease(gil);
    }

    bool success = false;
    if (result == 0) {
        std::ifstream reportFile(reportPath, std::ios::binary);
        if (reportFile) {
            std::ostringstream output;
            output << reportFile.rdbuf();
            report = output.str();
            success = !report.empty();
        }
        if (!success)
            error = "The compatibility checker did not produce a report.";
    } else {
        error = "The embedded Python compatibility checker could not run.";
    }

    std::error_code ignored;
    std::filesystem::remove(sourcePath, ignored);
    std::filesystem::remove(reportPath, ignored);
    return success;
#else
    (void)source;
    (void)report;
    error = "The portable embedded Python runtime is currently packaged for Windows.";
    return false;
#endif
}
