#include "python_runtime.h"
#include "lifecycle_trace.h"

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
    'interleave', 'transpose', 'euclid', 'PAlt', 'PStretch', 'PStutter',
    'PShuf', 'PJoin', 'PBeat', 'PDur', 'PDelay', 'PStrum', 'PQuicken', 'PRhythm',
    'PZip', 'PZip2',
    'play', 'sequence', 'saw', 'pulse', 'sine', 'triangle', 'bass', 'varsaw',
    'dirt', 'jbass', 'noise', 'dub', 'growl', 'soprano', 'crunch', 'lazer', 'rave',
    'bell', 'gong', 'marimba', 'pluck', 'twang', 'donk', 'squish',
    'charm', 'spark', 'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug',
    'ripple', 'viola', 'nylon', 'arpy', 'dab', 'sawbass', 'dbass',
    'prophet', 'pads', 'pasha', 'ambi', 'sinepad', 'keys', 'klank',
    'feel', 'glass', 'soft', 'quin', 'space', 'scratch', 'creep',
    'snick', 'swell', 'razz', 'sitar', 'star', 'karp',
    'var', 'linvar', 'Clock', 'Scale', 'Root', 'PGroup',
    'Player', 'PlayerPattern', 'MouseX', 'MouseY', '__build_class__'
}
_COMPAT_WAVES = {'saw', 'pulse', 'sine', 'triangle', 'bass', 'varsaw', 'dirt',
                 'jbass', 'noise', 'dub', 'growl', 'soprano', 'crunch', 'lazer', 'rave',
                 'bell', 'gong', 'marimba', 'pluck', 'twang', 'donk', 'squish',
                 'charm', 'spark', 'blip', 'scatter', 'orient', 'zap',
                 'fuzz', 'bug', 'ripple', 'viola', 'nylon', 'arpy', 'dab',
                 'sawbass', 'dbass', 'prophet', 'pads', 'pasha', 'ambi',
                 'sinepad', 'keys', 'klank', 'feel', 'glass', 'soft', 'quin',
                 'space', 'scratch', 'creep', 'snick', 'swell', 'razz',
                 'sitar', 'star', 'karp'}
_COMPAT_WAVE_KEYWORDS = {
    'notes', 'dur', 'oct', 'sus', 'scale', 'root', 'amp', 'pan', 'cutoff', 'echo', 'decay',
    'delay', 'delaytime', 'echomix', 'feedback', 'velocity', 'rate', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell', 'strum',
    'vib', 'depth', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur'
}
_COMPAT_SEQUENCE_KEYWORDS = {
    'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'synth',
    'amp', 'pan', 'cutoff', 'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell', 'root', 'strum', 'rate'
}
_COMPAT_NOTE_PLAY_KEYWORDS = {'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'strum'}
_COMPAT_SAMPLE_PLAY_KEYWORDS = (_COMPAT_SEQUENCE_KEYWORDS - {'notes', 'step', 'duration', 'degrees', 'synth', 'root', 'strum'}) | {'dur', 'sus', 'sample'}
_COMPAT_PLAY_KEYWORDS = _COMPAT_NOTE_PLAY_KEYWORDS | _COMPAT_SAMPLE_PLAY_KEYWORDS
_COMPAT_SAMPLE_SYMBOLS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&*@^:$=!/#-%+?~\\1234'
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

def _compat_validate_play_string(text):
    pairs = {'[':']', '(' :')', '{':'}', '<':'>'}
    stack = []
    symbols = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&*@^:$=!/#-%+?~\\1234'
    index = 0
    while index < len(text):
        character = text[index]
        if character == '|':
            closing = text.find('|', index + 1)
            if closing < 0:
                raise ValueError("missing closing '|' in sample-index selector; use |x1|")
            selector = text[index + 1:closing]
            if (len(selector) != 2 or selector[0] not in symbols or
                    selector[1] not in '0123456789'):
                raise ValueError('sample-index selector must contain one sample symbol and one digit, like |x1|')
            index = closing + 1
            continue
        if character in pairs:
            stack.append((pairs[character], character, index))
            if len(stack) > 8:
                raise ValueError('sample-string nesting is limited to 8 levels')
        elif character in ')]}>':
            if not stack or stack[-1][0] != character:
                raise ValueError('unmatched closing delimiter ' + character + ' in sample string')
            if index == stack[-1][2] + 1:
                raise ValueError('empty group in sample string')
            stack.pop()
        elif not character.isspace() and character not in symbols:
            raise ValueError('unsupported sample-string character ' + repr(character))
        index += 1
    if stack:
        raise ValueError('missing closing delimiter ' + stack[-1][0] + ' in sample string')

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

    def _pattern_kind(self, node):
        if isinstance(node, ast.Constant):
            if isinstance(node.value, str):
                return 'sample'
            if node.value is None or (
                    isinstance(node.value, (int, float)) and
                    not isinstance(node.value, bool)):
                return 'numeric'
            return None
        if isinstance(node, ast.Subscript) and isinstance(node.value, ast.Name) and node.value.id == 'P':
            values = node.slice.elts if isinstance(node.slice, ast.Tuple) else [node.slice]
            kinds = {kind for kind in (self._pattern_kind(value) for value in values)
                     if kind is not None}
            if 'mixed' in kinds or len(kinds) > 1:
                return 'mixed'
            return next(iter(kinds)) if kinds else None
        if isinstance(node, (ast.List, ast.Tuple)):
            kinds = {kind for kind in (self._pattern_kind(value) for value in node.elts)
                     if kind is not None}
            if 'mixed' in kinds or len(kinds) > 1:
                return 'mixed'
            return next(iter(kinds)) if kinds else None
        if isinstance(node, ast.BinOp) and isinstance(
                node.op, (ast.BitAnd, ast.BitOr)):
            kinds = {kind for kind in (
                self._pattern_kind(node.left), self._pattern_kind(node.right))
                     if kind is not None}
            if 'mixed' in kinds or len(kinds) > 1:
                return 'mixed'
            return next(iter(kinds)) if kinds else None
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in {'PZip', 'PZip2'}:
            limit = 8 if node.func.id == 'PZip' else 2
            arguments = list(node.args[:limit])
            arguments.extend(keyword.value for keyword in node.keywords
                             if keyword.arg in {'pat1', 'pat2'})
            kinds = {kind for kind in (self._pattern_kind(value) for value in arguments)
                     if kind is not None}
            if 'mixed' in kinds or len(kinds) > 1:
                return 'mixed'
            return next(iter(kinds)) if kinds else None
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
            return self._pattern_kind(node.func.value)
        return None

    def _finite_pattern_expression(self, node):
        if isinstance(node, ast.Subscript) and isinstance(
                node.value, ast.Name) and node.value.id == 'P':
            return self._pattern_kind(node) is not None
        if isinstance(node, ast.BinOp) and isinstance(
                node.op, (ast.BitAnd, ast.BitOr)):
            return (self._finite_pattern_expression(node.left) and
                    self._finite_pattern_expression(node.right))
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
            return self._finite_pattern_expression(node.func.value)
        return False

    def _check_pattern_kinds(self, nodes, line, helper_name):
        kinds = {self._pattern_kind(node) for node in nodes}
        kinds.discard(None)
        if 'mixed' in kinds or len(kinds) > 1:
            self.add(line, helper_name +
                     ' cannot mix numeric note patterns with sample-symbol patterns.')

    def visit_Import(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_ImportFrom(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_Call(self, node):
        name = node.func.id if isinstance(node.func, ast.Name) else None
        if (isinstance(node.func, ast.Attribute) and
                isinstance(node.func.value, ast.Name) and
                node.func.value.id == 'Clock' and node.func.attr == 'swing'):
            if len(node.args) > 1:
                self.add(node.lineno, 'Clock.swing accepts one optional amount.')
            for keyword in node.keywords:
                if keyword.arg is None:
                    self.add(keyword.lineno or node.lineno, 'Dynamic keyword expansion cannot be checked; use Clock.swing(amount).')
                elif keyword.arg != 'amount':
                    self.add(keyword.lineno or node.lineno, 'Clock.swing accepts only the amount keyword.')
        if name and name not in _COMPAT_CALLS and name not in self.defined:
            self.add(node.lineno, "Call '" + name + "()' is not in the embedded API (often an unsupported FoxDot synth or pattern helper).")
        if name == 'PAlt':
            if len(node.args) < 2:
                self.add(node.lineno, 'PAlt requires at least two finite patterns.')
            self._check_pattern_kinds(node.args, node.lineno, 'PAlt')
        if name in {'PStretch', 'PStutter', 'PShuf'}:
            if name == 'PStretch' and len(node.args) != 2:
                self.add(node.lineno, 'PStretch requires a finite pattern and a target length.')
            if name == 'PStutter' and not 1 <= len(node.args) <= 2:
                self.add(node.lineno, 'PStutter requires a finite pattern and optional repeat counts.')
            if name == 'PShuf' and len(node.args) != 1:
                self.add(node.lineno, 'PShuf requires exactly one finite pattern.')
            if node.args:
                self._check_pattern_kinds(node.args[:1], node.lineno, name)
        if name == 'PJoin':
            if len(node.args) != 1:
                self.add(node.lineno, 'PJoin requires one list or pattern of finite phrases.')
            else:
                self._check_pattern_kinds(node.args, node.lineno, 'PJoin')
        if name in {'PBeat', 'PDur'}:
            minimum = 1 if name == 'PBeat' else 2
            maximum = 3 if name == 'PBeat' else 4
            if not minimum <= len(node.args) + len(node.keywords) <= maximum:
                self.add(node.lineno, name + ' expects ' +
                         ('a pulse string' if name == 'PBeat' else 'pulse and step counts') +
                         ' with optional start and dur values.')
        if name == 'PDelay' and not 2 <= len(node.args) + len(node.keywords) <= 4:
            self.add(node.lineno, 'PDelay expects pulse and step counts with optional start and dur values.')
        if name == 'PStrum' and len(node.args) + len(node.keywords) > 1:
            self.add(node.lineno, 'PStrum accepts one optional subdivision count.')
        if name == 'PQuicken':
            allowed = {'dur', 'stepsize', 'steps'}
            if len(node.args) + len(node.keywords) > 3:
                self.add(node.lineno, 'PQuicken accepts dur, stepsize, and steps.')
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(keyword.lineno or node.lineno, 'PQuicken supports only dur, stepsize, and steps keywords.')
        if name == 'PRhythm' and (len(node.args) != 1 or node.keywords):
            self.add(node.lineno, 'PRhythm expects one finite list or P pattern.')
        if name == 'PZip':
            allowed = {'pat1', 'pat2'}
            if not 2 <= len(node.args) + len(node.keywords) <= 8:
                self.add(node.lineno, 'PZip requires 2 to 8 finite numeric or sample-symbol patterns.')
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(keyword.lineno or node.lineno, 'PZip accepts only pat1 and pat2 keywords; add further patterns positionally.')
            patternArguments = list(node.args[:8])
            patternArguments.extend(
                keyword.value for keyword in node.keywords
                if keyword.arg in {'pat1', 'pat2'})
            self._check_pattern_kinds(patternArguments, node.lineno, 'PZip')
        if name == 'PZip2':
            allowed = {'pat1', 'pat2', 'rule'}
            if not 2 <= len(node.args) + len(node.keywords) <= 3:
                self.add(node.lineno, 'PZip2 requires two finite patterns and an optional rule.')
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(keyword.lineno or node.lineno, 'PZip2 accepts only pat1, pat2, and rule keywords.')
            patternArguments = list(node.args[:2])
            patternArguments.extend(
                keyword.value for keyword in node.keywords
                if keyword.arg in {'pat1', 'pat2'})
            self._check_pattern_kinds(patternArguments, node.lineno, 'PZip2')
        keyword_names = None
        if name in _COMPAT_WAVES:
            keyword_names = _COMPAT_WAVE_KEYWORDS
        elif name == 'sequence':
            keyword_names = _COMPAT_SEQUENCE_KEYWORDS
        elif name == 'play':
            keyword_names = _COMPAT_PLAY_KEYWORDS
            if node.args and isinstance(node.args[0], ast.Constant):
                keyword_names = _COMPAT_SAMPLE_PLAY_KEYWORDS if isinstance(node.args[0].value, str) else _COMPAT_NOTE_PLAY_KEYWORDS
        if keyword_names is not None:
            for keyword in node.keywords:
                if keyword.arg is None:
                    self.add(node.lineno, 'Dynamic keyword expansion cannot be checked; use explicit supported controls.')
                elif keyword.arg not in keyword_names:
                    self.add(keyword.lineno or node.lineno, "Control '" + keyword.arg + "' is not supported by " + name + '() in this build.')
        if name in _COMPAT_WAVES and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            self.add(node.lineno, 'Use play(\"...\") for sample strings, not a pitched synth call.')
        if name == 'play' and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            try:
                _compat_validate_play_string(node.args[0].value)
            except ValueError as error:
                self.add(node.lineno, 'Unsupported FoxDot play string: ' + str(error))
        self.generic_visit(node)

    def visit_Attribute(self, node):
        if isinstance(node.value, ast.Name) and node.value.id == 'Clock' and node.attr not in {'bpm', 'meter', 'swing'}:
            self.add(node.lineno, "Clock." + node.attr + ' is not supported; use Clock.bpm, fixed 4/4 Clock.meter, or Clock.swing(...).')
        if (isinstance(node.value, ast.Name) and node.value.id == 'Clock' and
                node.attr == 'swing' and isinstance(node.ctx, ast.Store)):
            self.add(node.lineno, 'Clock.swing is a method; call Clock.swing(amount), not assign to it.')
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
                items = node.slice.elts if isinstance(node.slice, ast.Tuple) else [node.slice]
                valid_sample_symbols = all(
                    isinstance(item, ast.Constant) and isinstance(item.value, str) and
                    len(item.value) == 1 and
                    (item.value == ' ' or item.value in _COMPAT_SAMPLE_SYMBOLS)
                    for item in items)
                if not valid_sample_symbols:
                    self.add(node.lineno, 'P sample-symbol literals require one sample symbol or a single-space rest per item.')
        self.generic_visit(node)

    def visit_BinOp(self, node):
        has_sample_pattern = any(
            isinstance(item, ast.Subscript) and
            isinstance(item.value, ast.Name) and item.value.id == 'P' and
            any(isinstance(value, ast.Constant) and isinstance(value.value, str)
                for value in ast.walk(item.slice))
            for item in ast.walk(node))
        if has_sample_pattern and isinstance(
                node.op, (ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv,
                          ast.Mod, ast.Pow, ast.BitXor)):
            self.add(node.lineno, 'P arithmetic over sample-symbol patterns is not supported; use reverse(), rotate(), or | to concatenate.')
        if has_sample_pattern and isinstance(node.op, ast.BitAnd):
            self._check_pattern_kinds(
                [node.left, node.right], node.lineno, 'P &')
            if ((self._pattern_kind(node.left) == 'sample' or
                 self._pattern_kind(node.right) == 'sample') and
                    not (self._finite_pattern_expression(node.left) and
                         self._finite_pattern_expression(node.right))):
                self.add(node.lineno, 'Sample-symbol & requires two finite P sample-symbol patterns.')
        elif isinstance(node.op, (ast.BitOr, ast.BitAnd)) and any(
                isinstance(item, ast.Constant) and isinstance(item.value, str)
                for item in ast.walk(node)) and not has_sample_pattern:
            self.add(node.lineno, 'Pattern | and & do not accept sample strings; use P sample-symbol patterns for concatenation.')
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
            lines.append('Supported synth calls: ' + ', '.join(sorted(_COMPAT_WAVES)) + '. WAV play strings support symbols, rests, [], (), {}, adjacent <> layers, and per-hit sample selectors like |x1|.')
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
_dexter_synth_controls = {}
_dexter_sample_events = {}
_dexter_sample_controls = {}
_dexter_shuffle_counter = 0
_dexter_swing_seconds = 0.0
_NEW_SYNTHS = ('viola', 'nylon', 'arpy', 'dab', 'sawbass', 'dbass',
               'prophet', 'pads', 'pasha', 'ambi', 'sinepad', 'keys',
               'klank', 'feel', 'glass', 'soft', 'quin', 'space',
               'scratch', 'creep', 'snick', 'swell', 'razz', 'sitar',
               'star', 'karp')
def tempo(bpm):
    global _dexter_bpm
    bpm = float(bpm)
    if bpm != bpm or bpm in (float('inf'), float('-inf')) or bpm < 0.001:
        raise ValueError('tempo must be finite and at least 0.001 BPM')
    if abs(_dexter_swing_seconds * bpm / 60.0) >= 0.49:
        raise ValueError('Clock.swing would shift notes by 0.49 beats or more at this tempo')
    _dexter_bpm = bpm

class _FoxDotClock:
    def __getattr__(self, name):
        if name == 'bpm':
            return _dexter_bpm
        if name == 'meter':
            return (4, 4)
        raise ValueError('Clock.' + str(name) + ' is not supported in the embedded clock')
    def swing(self, amount=0.1):
        global _dexter_swing_seconds
        amount = float(amount)
        if amount != amount or amount in (float('inf'), float('-inf')):
            raise ValueError('Clock.swing amount must be finite')
        nudge = amount * (_dexter_bpm / 120.0)
        if abs(nudge * _dexter_bpm / 60.0) >= 0.49:
            raise ValueError('Clock.swing would shift notes by 0.49 beats or more at this tempo')
        _dexter_swing_seconds = nudge
        for _player in _dexter_updates:
            _update = _dexter_updates[_player]
            _dexter_updates[_player] = _update[:-1] + (_dexter_swing_seconds,)
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
            raise TypeError('P pattern arithmetic is limited to numeric notes, rests, and chords; use reverse(), rotate(), or | for sample-symbol patterns')
        return items

    def _sample_sequence(self, value):
        if isinstance(value, _FoxDotFinitePattern):
            items = list(value)
        elif isinstance(value, str):
            items = list(value)
        elif isinstance(value, (list, tuple)):
            items = list(value)
        else:
            return None
        if not items:
            return None
        if all(_sample_symbols_for_item(item) is not None for item in items):
            return items
        return None

    def _concatenate(self, left_value, right_value):
        left_sample = self._sample_sequence(left_value)
        right_sample = self._sample_sequence(right_value)
        if left_sample is not None or right_sample is not None:
            if left_sample is None and isinstance(left_value, (list, tuple, str)) and not left_value:
                left_sample = []
            if right_sample is None and isinstance(right_value, (list, tuple, str)) and not right_value:
                right_sample = []
            if left_sample is None or right_sample is None:
                raise TypeError('P sample-symbol concatenation requires sample symbols or space-rests on both sides')
            if len(left_sample) + len(right_sample) > 256:
                raise ValueError('P sample-symbol concatenation is limited to 256 items')
            return _FoxDotFinitePattern(left_sample + right_sample)
        left = self._composition_items(left_value)
        right = self._composition_items(right_value)
        if len(left) + len(right) > 256:
            raise ValueError('P pattern concatenation is limited to 256 items')
        return _FoxDotFinitePattern(left + right)

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

    def _zip_sample_item(self, left, right):
        symbols = []
        for item in (left, right):
            members = _sample_symbols_for_item(item)
            if members is None:
                raise TypeError('P sample-symbol & requires finite sample-symbol patterns')
            symbols.extend(members)
            if len(symbols) > 8:
                raise ValueError('zipped sample groups can contain at most 8 symbols')
        if not symbols:
            return ' '
        return symbols[0] if len(symbols) == 1 else tuple(symbols)

    def _zip_with(self, other, reflected=False):
        left_value = other if reflected else self
        right_value = self if reflected else other
        left_sample = (self._sample_sequence(left_value)
                       if isinstance(left_value, _FoxDotFinitePattern) else None)
        right_sample = (self._sample_sequence(right_value)
                        if isinstance(right_value, _FoxDotFinitePattern) else None)
        if left_sample is not None or right_sample is not None:
            if left_sample is None or right_sample is None:
                raise TypeError('P & cannot mix numeric note patterns with sample-symbol patterns')
            if not left_sample or not right_sample:
                return _FoxDotFinitePattern()
            length = self._laced_length(left_sample, right_sample)
            if length > 256:
                raise ValueError('P sample-symbol zipping is limited to 256 expanded items')
            return _FoxDotFinitePattern([
                self._zip_sample_item(
                    left_sample[index % len(left_sample)],
                    right_sample[index % len(right_sample)])
                for index in range(length)
            ])
        left = self._composition_items(left_value)
        right = self._composition_items(right_value)
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
        return self._concatenate(self, other)

    def __ror__(self, other):
        return self._concatenate(other, self)

    def __and__(self, other):
        return self._zip_with(other)

    def __rand__(self, other):
        return self._zip_with(other, True)

def _finite_note_number(value):
    return (isinstance(value, (int, float)) and not isinstance(value, bool) and
            value == value and value not in (float('inf'), float('-inf')))

def _numeric_pattern_item(value):
    if value is None or _finite_note_number(value):
        return True
    if isinstance(value, (list, tuple)):
        return 1 <= len(value) <= 8 and all(_finite_note_number(note) for note in value)
    return False

def _pattern_items(pattern, name):
    if isinstance(pattern, str):
        values = list(pattern)
    elif isinstance(pattern, (list, tuple, _FoxDotFinitePattern)):
        values = list(pattern)
    else:
        raise TypeError(name + ' expects a finite numeric or sample-symbol pattern')
    if len(values) > 256:
        raise ValueError(name + ' input patterns are limited to 256 items')
    if not values:
        return values, None
    if all(isinstance(item, str) and len(item) == 1 and
           (item == ' ' or item in _SAMPLE_SYMBOLS) for item in values):
        return values, 'sample'
    if all(_numeric_pattern_item(item) for item in values):
        return values, 'numeric'
    raise TypeError(name +
                    ' requires either numeric notes/rests/chords or sample symbols/space-rests; do not mix them')

def _compatible_pattern_sequences(patterns, name, minimum=1):
    if len(patterns) < minimum:
        raise ValueError(name + ' requires at least ' + str(minimum) + ' finite patterns')
    sequences = []
    kind = None
    for pattern in patterns:
        values, patternKind = _pattern_items(pattern, name)
        if not values:
            raise ValueError(name + ' patterns cannot be empty')
        if kind is not None and patternKind != kind:
            raise TypeError(name + ' cannot mix numeric note patterns with sample-symbol patterns')
        kind = patternKind
        sequences.append(values)
    return sequences, kind

def PAlt(*patterns):
    sequences, kind = _compatible_pattern_sequences(patterns, 'PAlt', 2)
    probe = _FoxDotFinitePattern()
    length = 1
    for values in sequences:
        length = length * len(values) // probe._gcd(length, len(values))
        if length * len(sequences) > 256:
            raise ValueError('PAlt is limited to 256 expanded items')
    return _FoxDotFinitePattern([
        sequences[patternIndex][step % len(sequences[patternIndex])]
        for step in range(length)
        for patternIndex in range(len(sequences))
    ])

def PJoin(patterns):
    if not isinstance(patterns, (str, list, tuple, _FoxDotFinitePattern)):
        raise TypeError('PJoin expects a list or pattern of finite phrases')
    if len(patterns) > 256:
        raise ValueError('PJoin is limited to 256 input phrases')
    result = []
    kind = None
    for pattern in patterns:
        values, patternKind = _pattern_items(pattern, 'PJoin')
        if patternKind is not None and kind is not None and patternKind != kind:
            raise TypeError('PJoin cannot mix numeric note patterns with sample-symbol patterns')
        if patternKind is not None:
            kind = patternKind
        if len(result) + len(values) > 256:
            raise ValueError('PJoin output is limited to 256 items')
        result.extend(values)
    return _FoxDotFinitePattern(result)

def _pulses_to_durations(pulses):
    count = 1
    result = []
    for pulse in pulses[1:]:
        if pulse == 1:
            result.append(count)
            count = 1
        else:
            count += 1
    result.append(count)
    return result

def _rotate_foxdot_durations(values, amount):
    items = list(values)
    return _FoxDotFinitePattern(items[amount:] + items[:amount])

def _scale_foxdot_durations(values, dur, name):
    if isinstance(dur, _DexterTimeVar):
        raise TypeError(name + ' dur must be a positive number or finite numeric pattern')
    if isinstance(dur, (list, tuple, _FoxDotFinitePattern)):
        scales = list(dur)
        if not scales or len(scales) > 256:
            raise ValueError(name + ' dur pattern must contain 1 to 256 values')
    else:
        scales = [dur]
    for scale in scales:
        if not isinstance(scale, (int, float)):
            raise TypeError(name + ' dur must be a positive number or finite numeric pattern')
        numeric = float(scale)
        if numeric != numeric or numeric in (float('inf'), float('-inf')) or numeric <= 0.0:
            raise ValueError(name + ' dur values must be finite and positive')
    result = _FoxDotFinitePattern(values) * dur
    if not result or len(result) > 256:
        raise ValueError(name + ' output is limited to 256 duration values')
    total = 0.0
    for value in result:
        numeric = float(value)
        if numeric != numeric or numeric in (float('inf'), float('-inf')) or not 0.001 <= numeric <= 4096.0:
            raise ValueError(name + ' durations must be between 0.001 and 4096 beats')
        total += numeric
    if total > 4096.0:
        raise ValueError(name + ' total duration is limited to 4096 beats')
    return _FoxDotFinitePattern(result)

def PBeat(string, start=0, dur=0.5):
    if not isinstance(string, str):
        raise TypeError('PBeat expects a pulse string')
    if len(string) > 256:
        raise ValueError('PBeat pulse strings are limited to 256 characters')
    try:
        rotation = int(start)
    except (TypeError, ValueError, OverflowError):
        raise TypeError('PBeat start must be an integer')
    pulses = [1 if character != ' ' else 0 for character in string]
    durations = _pulses_to_durations(pulses)
    return _scale_foxdot_durations(
        _rotate_foxdot_durations(durations, rotation), dur, 'PBeat')

def _foxdot_euclidean_pulses(hits, steps):
    if hits == 0:
        return [0 for _ in range(steps)]
    groups = [[1 if index < hits else 0] for index in range(steps)]
    remaining = steps
    while True:
        remaining -= hits
        if remaining <= 1:
            break
        if remaining < hits:
            hits, remaining = remaining, hits
        for index in range(hits):
            groups[index].extend(groups[-1])
            del groups[-1]
    return [pulse for group in groups for pulse in group]

def PDur(n, k, start=0, dur=0.25):
    if isinstance(n, bool) or not isinstance(n, int):
        raise TypeError('PDur pulse count must be an integer')
    if isinstance(k, bool) or not isinstance(k, int):
        raise TypeError('PDur step count must be an integer')
    if n < 0 or n > 256 or k < 1 or k > 256:
        raise ValueError('PDur requires 0 <= pulses <= 256 and 1 <= steps <= 256')
    scale = 1.0
    while n > k:
        k *= 2
        scale *= 0.5
        if k > 256:
            raise ValueError('PDur expansion is limited to 256 steps')
    try:
        rotation = int(start)
    except (TypeError, ValueError, OverflowError):
        raise TypeError('PDur start must be an integer')
    durations = _pulses_to_durations(_foxdot_euclidean_pulses(n, k))
    durations = [value * scale for value in durations]
    return _scale_foxdot_durations(
        _rotate_foxdot_durations(durations, rotation), dur, 'PDur')

def PDelay(n, k, start=0, dur=0.25):
    durations = PDur(n, k, start, dur)
    offsets = [0.0]
    for duration in durations[:-1]:
        offsets.append(offsets[-1] + duration)
    return _FoxDotFinitePattern(offsets)

def PStrum(n=4):
    if isinstance(n, bool) or not isinstance(n, int):
        raise TypeError('PStrum subdivision count must be an integer')
    if n < 0 or n > 15:
        raise ValueError('PStrum subdivision count must be between 0 and 15')
    first = _FoxDotFinitePattern([1.0, 0.5]).stutter([1, n + 1])
    second = _FoxDotFinitePattern([1.5, 0.5]).stutter([1, n])
    return first | second | 1.0

def _pzip_pattern_values(pattern, name):
    if isinstance(pattern, _FoxDotFinitePattern):
        values = list(pattern)
    elif isinstance(pattern, (list, tuple)):
        values = list(pattern)
    elif pattern is None or (isinstance(pattern, (int, float)) and
                             not isinstance(pattern, bool)):
        values = [pattern]
    else:
        raise TypeError(name +
                        ' accepts finite numeric notes/rests or sample symbols/space-rests')
    if not values:
        raise ValueError(name + ' patterns cannot be empty')
    if len(values) > 256:
        raise ValueError(name + ' input patterns are limited to 256 items')
    kind = None
    for item in values:
        if item is None or _finite_note_number(item):
            item_kind = 'numeric'
        elif _sample_symbols_for_item(item) is not None:
            item_kind = 'sample'
        else:
            raise TypeError(name +
                            ' accepts finite numeric notes/rests or sample symbols/space-rests, not nested numeric chords')
        if kind is not None and item_kind != kind:
            raise TypeError(name +
                            ' cannot mix numeric note patterns with sample-symbol patterns')
        kind = item_kind
    return values, kind

def _pzip_laced_length(patterns, name):
    length = 1
    for pattern in patterns:
        other = len(pattern)
        left, right = length, other
        while right:
            left, right = right, left % right
        length = (length // left) * other
        if length > 256:
            raise ValueError(name + ' output is limited to 256 laced items')
    return length

def PZip(pat1, pat2, *patN):
    parsed = [_pzip_pattern_values(pattern, 'PZip')
              for pattern in (pat1, pat2) + patN]
    if len(parsed) > 8:
        raise ValueError('PZip accepts 2 to 8 patterns')
    kinds = {kind for _, kind in parsed}
    if len(kinds) > 1:
        raise TypeError('PZip cannot mix numeric note patterns with sample-symbol patterns')
    patterns = [values for values, _ in parsed]
    length = _pzip_laced_length(patterns, 'PZip')
    if kinds == {'sample'}:
        zipper = _FoxDotFinitePattern()
        zipped = []
        for index in range(length):
            item = patterns[0][index % len(patterns[0])]
            for pattern in patterns[1:]:
                item = zipper._zip_sample_item(item, pattern[index % len(pattern)])
            zipped.append(item)
        return _FoxDotFinitePattern(zipped)
    return _FoxDotFinitePattern([
        tuple(pattern[index % len(pattern)] for pattern in patterns)
        for index in range(length)])

def PZip2(pat1, pat2, rule=None):
    parsed = [_pzip_pattern_values(pat1, 'PZip2'),
              _pzip_pattern_values(pat2, 'PZip2')]
    if parsed[0][1] != parsed[1][1]:
        raise TypeError('PZip2 cannot mix numeric note patterns with sample-symbol patterns')
    patterns = [values for values, _ in parsed]
    length = _pzip_laced_length(patterns, 'PZip2')
    result = []
    sample_zipper = _FoxDotFinitePattern() if parsed[0][1] == 'sample' else None
    for index in range(length):
        left = patterns[0][index % len(patterns[0])]
        right = patterns[1][index % len(patterns[1])]
        if rule is None or rule(left, right):
            result.append(sample_zipper._zip_sample_item(left, right)
                          if sample_zipper is not None else (left, right))
            if len(result) > 256:
                raise ValueError('PZip2 output is limited to 256 zipped pairs')
    return _FoxDotFinitePattern(result)

class _FoxDotDurationGroup(tuple):
    def __new__(cls, values):
        return tuple.__new__(cls, values)

def PQuicken(dur=0.5, stepsize=3, steps=6):
    if isinstance(dur, bool) or not isinstance(dur, (int, float)):
        raise TypeError('PQuicken dur must be a positive number of beats')
    dur = float(dur)
    if dur != dur or dur in (float('inf'), float('-inf')) or not 0.0 < dur <= 8.0:
        raise ValueError('PQuicken dur must be greater than 0 and at most 8 beats')
    if isinstance(stepsize, bool) or not isinstance(stepsize, int):
        raise TypeError('PQuicken stepsize must be an integer')
    if isinstance(steps, bool) or not isinstance(steps, int):
        raise TypeError('PQuicken steps must be an integer')
    if stepsize < 2 or stepsize > 257 or steps < 1 or steps > 256:
        raise ValueError('PQuicken requires 2 <= stepsize <= 257 and 1 <= steps <= 256')
    if (stepsize - 1) * steps > 256:
        raise ValueError('PQuicken output is limited to 256 delay offsets')
    delays = []
    count = 0.0
    for _ in range(steps):
        for _ in range(stepsize - 1):
            delays.append(count)
            count += dur / stepsize
        dur /= stepsize
    return _FoxDotFinitePattern(delays)

def PRhythm(durations):
    if not isinstance(durations, (list, tuple, _FoxDotFinitePattern)):
        raise TypeError('PRhythm expects a finite list or P pattern')
    values = list(durations)
    if not values or len(values) > 256:
        raise ValueError('PRhythm requires 1 to 256 duration items')
    if len(values) == 1 and isinstance(values[0], _FoxDotDurationGroup):
        return _FoxDotFinitePattern(values)
    if len(values) == 1 and isinstance(values[0], tuple):
        if not 2 <= len(values[0]) <= 4:
            raise ValueError('PRhythm Euclidean tuples require 2 to 4 PDur arguments')
        return PDur(*values[0])

    expandedItems = [0]

    def parseItem(item, depth):
        if depth > 8:
            raise ValueError('PRhythm nesting is limited to 8 levels')
        if isinstance(item, _FoxDotDurationGroup):
            if not item:
                raise ValueError('PRhythm duration groups cannot be empty')
            base = float(item[0])
            offsets = [float(value) for value in item[1:]]
            if (base != base or base < 0.001 or base > 4096.0 or
                    any(value != value or value < 0.0 or value > 8.0 or value > base
                        for value in offsets)):
                raise ValueError('PRhythm duration-group offsets must fit the 8-beat delay range')
            expandedItems[0] += len(item)
            return item, base
        if isinstance(item, tuple):
            if not 2 <= len(item) <= 4:
                raise ValueError('PRhythm Euclidean tuples require 2 to 4 PDur arguments')
            euclideanDurations = PDur(*item)
            baseDuration = sum(euclideanDurations)
            offsets = []
            elapsed = 0.0
            for duration in euclideanDurations[:-1]:
                elapsed += duration
                offsets.append(elapsed)
            if baseDuration > 4096.0 or any(value > 8.0 for value in offsets):
                raise ValueError('PRhythm grouped offsets must fit the 8-beat delay range')
            expandedItems[0] += len(euclideanDurations)
            return _FoxDotDurationGroup([baseDuration] + offsets), baseDuration
        if isinstance(item, (int, float)) and not isinstance(item, bool):
            value = float(item)
            if value != value or value in (float('inf'), float('-inf')) or not 0.001 <= value <= 4096.0:
                raise ValueError('PRhythm durations must be finite values from 0.001 to 4096 beats')
            expandedItems[0] += 1
            return value, value
        if isinstance(item, (list, _FoxDotFinitePattern)):
            children = list(item)
            if not children or len(children) > 256:
                raise ValueError('PRhythm nested patterns require 1 to 256 items')
            parsed = [parseItem(child, depth + 1) for child in children]
            if len(parsed) == 1:
                return parsed[0]
            offsets = []
            elapsed = 0.0
            for child, span in parsed:
                if isinstance(child, _FoxDotDurationGroup):
                    if elapsed > 0.0:
                        offsets.append(elapsed)
                    offsets.extend(elapsed + float(value) for value in child[1:])
                elif elapsed > 0.0:
                    offsets.append(elapsed)
                elapsed += span
            if elapsed > 4096.0 or any(
                    value > 8.0 or value > elapsed for value in offsets):
                raise ValueError('PRhythm nested offsets must fit the 8-beat delay range')
            return _FoxDotDurationGroup([elapsed] + offsets), elapsed
        raise TypeError(
            'PRhythm supports numbers, nested duration patterns, and (n, k[, start[, dur]]) tuples')

    output = []
    totalBeats = 0.0
    for item in values:
        parsed, span = parseItem(item, 0)
        output.append(parsed)
        totalBeats += span
        if expandedItems[0] > 256 or totalBeats > 4096.0:
            raise ValueError('PRhythm is limited to 256 expanded items and 4096 total beats')
    return _FoxDotFinitePattern(output)

def PStretch(pattern, size):
    values, kind = _pattern_items(pattern, 'PStretch')
    if isinstance(size, bool) or not isinstance(size, int):
        raise TypeError('PStretch target length must be an integer')
    if size < 0:
        raise ValueError('PStretch target length cannot be negative')
    if size > 256:
        raise ValueError('PStretch output is limited to 256 items')
    if size and not values:
        raise ValueError('PStretch cannot stretch an empty pattern')
    return _FoxDotFinitePattern([
        values[index % len(values)] for index in range(size)
    ]) if size else _FoxDotFinitePattern()

def PStutter(pattern, counts=2):
    values, kind = _pattern_items(pattern, 'PStutter')
    if isinstance(counts, _FoxDotFinitePattern):
        repeats = list(counts)
    elif isinstance(counts, (list, tuple)):
        repeats = list(counts)
    else:
        repeats = [counts]
    if not repeats:
        raise ValueError('PStutter repeat counts cannot be empty')
    for repeat_count in repeats:
        if isinstance(repeat_count, bool) or not isinstance(repeat_count, int):
            raise TypeError('PStutter counts must be whole numbers from 0 to 16')
        if not 0 <= repeat_count <= 16:
            raise ValueError('PStutter counts must be whole numbers from 0 to 16')
    if len(repeats) > 256:
        raise ValueError('PStutter repeat-count patterns are limited to 256 items')
    if not values:
        return _FoxDotFinitePattern()
    iterationCount = _FoxDotFinitePattern()._laced_length(values, repeats)
    if iterationCount > 256:
        raise ValueError('PStutter is limited to 256 source/count pattern steps')
    result = []
    for index in range(iterationCount):
        repeat_count = repeats[index % len(repeats)]
        if len(result) + repeat_count > 256:
            raise ValueError('PStutter output is limited to 256 items')
        result.extend([values[index % len(values)]] * repeat_count)
    return _FoxDotFinitePattern(result)

def PShuf(pattern):
    global _dexter_shuffle_counter
    values, kind = _pattern_items(pattern, 'PShuf')
    shuffled = list(values)
    _dexter_shuffle_counter += 1
    state = (int(_dexter_shuffle_seed) +
             _dexter_shuffle_counter * 1013904223) & 0xffffffff
    for item in values:
        encoded = item if isinstance(item, str) else str(item)
        for character in encoded:
            state = ((state ^ ord(character)) * 16777619) & 0xffffffff
    for index in range(len(shuffled) - 1, 0, -1):
        state = (1664525 * state + 1013904223) & 0xffffffff
        target = state % (index + 1)
        shuffled[index], shuffled[target] = shuffled[target], shuffled[index]
    return _FoxDotFinitePattern(shuffled)

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

def _timing_pattern(value, count, name, allowGroups=False,
                    allowSustainGroups=False):
    if count == 0:
        return ([], []) if allowGroups or allowSustainGroups else []
    values = ([value] if allowSustainGroups and isinstance(value, _FoxDotChord)
              else [value] if isinstance(value, (int, float)) else list(value))
    if not values:
        raise ValueError(name + ' pattern cannot be empty')
    result = []
    groupOffsets = []
    sustainGroups = []
    for index in range(count):
        item = values[index % len(values)]
        if allowSustainGroups and isinstance(item, _FoxDotChord):
            group = [float(member) for member in item]
            if not group:
                raise ValueError('sus groups cannot be empty')
            result.append(min(group))
            sustainGroups.append(group)
            groupOffsets.append([])
        elif isinstance(item, _FoxDotDurationGroup):
            if not allowGroups:
                raise TypeError('PRhythm duration groups are supported in dur=, not sus=')
            if not item:
                raise ValueError('PRhythm duration groups cannot be empty')
            base = float(item[0])
            offsets = [float(offset) for offset in item[1:]]
            if (len(offsets) > 255 or
                    any(offset != offset or offset < 0.0 or offset > 8.0 or offset > base
                        for offset in offsets)):
                raise ValueError('PRhythm group offsets must be within the 8-beat delay range and its base duration')
            result.append(base)
            groupOffsets.append(offsets)
        else:
            numeric = float(item)
            result.append(numeric)
            groupOffsets.append([])
            if allowSustainGroups:
                sustainGroups.append([numeric])
    if allowSustainGroups:
        return result, sustainGroups
    return (result, groupOffsets) if allowGroups else result

def _play_notes(notes, step=1.0, duration=None, velocity=100, offset=0.0, degrees=False,
         strum=0.0,
         _octave_control=None, _base_octave=5, _degrees_per_octave=7,
         _attack_control=None, _release_control=None,
         _attack_default=0.01, _release_default=0.04, _time_multipliers=None,
         _allow_repeated_notes=False):
    global _dexter_length, _dexter_slots, _dexter_default_touched
    _dexter_default_touched = True
    if isinstance(notes, _FoxDotChord):
        notes = [notes]
    elif isinstance(notes, (int, float)):
        notes = [notes]
    if isinstance(notes, str):
        raise ValueError('FoxDot sample strings are not included yet; sample support is deferred')
    notes = list(notes)
    timeMultipliers = [1.0] * len(notes) if _time_multipliers is None else list(_time_multipliers)
    if len(timeMultipliers) != len(notes):
        raise ValueError('internal sample timing map does not match the event slots')
    stepIsVar = isinstance(step, _DexterTimeVar)
    if stepIsVar:
        steps = None
        stepGroupOffsets = [[] for _ in notes]
    else:
        steps, stepGroupOffsets = _timing_pattern(
            step, len(notes), 'dur', allowGroups=True)
    durationSource = duration
    durationIsVar = isinstance(durationSource, _DexterTimeVar)
    sustainGroups = None
    if not durationIsVar and durationSource is not None:
        _, sustainGroups = _timing_pattern(
            durationSource, len(notes), 'sus', allowSustainGroups=True)
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
    sustainGroupValues = []
    start = offset
    for i in range(len(notes)):
        multiplier = float(timeMultipliers[i])
        if multiplier != multiplier or not 0.0 < multiplier <= 1.0:
            raise ValueError('internal sample timing subdivisions must be in (0, 1]')
        baseStepValue = step.at(start) if stepIsVar else steps[i]
        stepValue = baseStepValue * multiplier
        stepGroupOffsets[i] = [
            delayOffset * multiplier for delayOffset in stepGroupOffsets[i]]
        if durationSource is None:
            durationValue = stepValue
            sustainValues = [durationValue]
        elif durationIsVar:
            durationValue = durationSource.at(start)
            sustainValues = [durationValue]
        else:
            sustainValues = sustainGroups[i]
        if stepValue != stepValue or stepValue < 0.001 or stepValue > 4096.0:
            raise ValueError('subdivided dur values must be between 0.001 and 4096 beats')
        if any(value != value or value in (float('inf'), float('-inf')) or
               value < 0.02 for value in sustainValues):
            raise ValueError('each sus value must be finite and at least 0.02 beats')
        starts.append(start)
        stepValues.append(stepValue)
        sustainGroupValues.append(sustainValues)
        start += stepValue
    totalBeats = sum(stepValues)
    if offset + totalBeats > 4096.0:
        raise ValueError('offset and pattern length must fit within 4096 beats')
    polyphonyLength = max(_dexter_length, offset + totalBeats)
    maxSustain = min(4096.0, polyphonyLength * 8.0)
    if any(value > maxSustain + 1e-9
           for group in sustainGroupValues for value in group):
        raise ValueError('sus cannot exceed 8 cycles of the pattern or 4096 beats')
    additions = []
    for i, item in enumerate(notes):
        start = starts[i]
        noteVelocity = int(round(velocity.at(start))) if velocityIsVar else velocity
        if not 1 <= noteVelocity <= 127:
            raise ValueError('velocity values must be from 1 to 127')
        if item is None:
            continue
        chord = list(item) if isinstance(item, (list, tuple)) else [item]
        chord = [note for note in chord if note is not None]
        if not chord:
            continue
        if len(chord) > 8:
            raise ValueError('a chord can contain at most 8 notes')
        strumValue = strum.at(start) if strumIsVar else strum
        offsets = [0.0] + stepGroupOffsets[i]
        sustainValues = sustainGroupValues[i]
        groupCount = max(len(chord), len(offsets), len(sustainValues))
        for groupIndex in range(groupCount):
            chordIndex = groupIndex % len(chord)
            note = chord[chordIndex]
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
            groupOffset = offsets[groupIndex % len(offsets)]
            sustainValue = sustainValues[groupIndex % len(sustainValues)]
            additions.append((start, sustainValue, note, noteVelocity,
                              attackValue, releaseValue,
                              strumValue * chordIndex + groupOffset,
                              bool(_allow_repeated_notes or
                                   stepGroupOffsets[i] or
                                   len(sustainValues) > 1),
                              i, chordIndex))
    if len(_dexter_events) + len(additions) > 256:
        raise ValueError('a pattern is limited to 256 note events')
    candidate = _dexter_events + additions
    boundaries = []
    active = 0
    for event in candidate:
        durationValue = event[1]
        fullCycles = int(durationValue // polyphonyLength)
        active += fullCycles
        if active > 8:
            raise ValueError('pattern exceeds 8-note polyphony across overlapping sustains')
        remainder = durationValue - fullCycles * polyphonyLength
        if remainder <= 1e-9:
            continue
        startBeat = (event[0] + event[6]) % polyphonyLength
        endBeat = startBeat + remainder
        if endBeat <= polyphonyLength:
            boundaries.extend(((startBeat, 1), (endBeat, -1)))
        else:
            boundaries.extend(((0.0, 1), (endBeat - polyphonyLength, -1),
                               (startBeat, 1), (polyphonyLength, -1)))
    for beat, delta in sorted(boundaries, key=lambda item: (item[0], item[1])):
        active += delta
        if active > 8:
            raise ValueError('pattern exceeds 8-note polyphony near beat ' + str(round(beat, 3)))
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

def _note_delay_control(value):
    if isinstance(value, _FoxDotFinitePattern):
        if not value or len(value) > 256:
            raise ValueError('delay pattern must contain 1 to 256 offsets')
        offsets = [float(item) for item in value]
        if any(item != item or item < 0.0 or item > 8.0 for item in offsets):
            raise ValueError('delay pattern offsets must be between 0 and 8 beats')
        return offsets[0], _FoxDotFinitePattern(offsets)
    return _event_control(value, 'delay', 0.0, 8.0)

class PlayerPattern:
    def __init__(self, notes, step=1.0, duration=None, velocity=100,
                 offset=0.0, degrees=False, synth='sine', amp=1.0, pan=0.0,
                 cutoff=20000.0, echo=0.0, delay=0.0, scaleName=None,
                 atk=0.01, rel=0.04, octaveControl=None, baseOctave=5, degreesPerOctave=7,
                 reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                   lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, synthRate=None, bpnoise=0.0, swell=0.0,
                    timeMultipliers=None, allowRepeatedNotes=False):
        self.notes = notes
        self.step = step
        self.timeMultipliers = timeMultipliers
        self.allowRepeatedNotes = bool(allowRepeatedNotes)
        self.duration = duration
        self.velocity = velocity
        self.offset = offset
        self.degrees = degrees
        self.rootMidi = None if root is None else _root_midi(root)
        if synth not in ('sine', 'triangle', 'saw', 'square', 'bass', 'varsaw',
                         'dirt', 'jbass', 'noise', 'dub', 'growl', 'soprano',
                         'crunch', 'lazer', 'rave', 'bell', 'gong', 'marimba',
                         'pluck', 'twang', 'donk', 'squish', 'charm', 'spark',
                         'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug',
                         'ripple') + _NEW_SYNTHS:
            raise ValueError('Unknown embedded synth: ' + str(synth))
        self.synth = synth
        self.synthControls = (0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0)
        if synthRate is None:
            synthRate = 1.0 if synth == 'bug' else 0.0
        try:
            self.synthRate = float(synthRate)
        except (TypeError, ValueError, OverflowError):
            raise ValueError('rate must be a finite number for the embedded synths')
        if self.synthRate != self.synthRate or not 0.0 <= self.synthRate <= 10.0:
            raise ValueError('rate must be between 0 and 10 for the embedded synths')
        if synth == 'varsaw' and self.synthRate > 1.0:
            raise ValueError('varsaw rate is pulse width and must be between 0 and 1')
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
        self.delayBeats, self.delayControl = _note_delay_control(delay)
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
        self.bpnoise, self.bpnoiseControl = _event_control(bpnoise, 'bpnoise', 0.0, 10.0)
        self.swell, self.swellControl = _event_control(swell, 'swell', 0.0, 4.0)
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
                      lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, rate=None, bpnoise=0.0, swell=0.0, _time_multipliers=None, _allow_repeated_notes=False):
    if isinstance(notes, _FoxDotChord):
        notes = [notes]
    if rate is None:
        rate = 8.0 if synth == 'bass' else 0.0
    return PlayerPattern(notes, step, duration, velocity, offset, degrees,
                        synth, amp, pan, cutoff, echo, delay, atk=atk, rel=rel,
                        reverb=reverb, room=room, mix=mix, damp=damp,
                           revtime=revtime, spread=spread, lpf=lpf, lpr=lpr, hpf=hpf, hpr=hpr, bpf=bpf, bpr=bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, synthRate=rate, bpnoise=bpnoise, swell=swell, timeMultipliers=_time_multipliers, allowRepeatedNotes=_allow_repeated_notes)

def _fox_wave(synthName, notes=0, dur=1.0, oct=5, sus=None, scale=None,
              amp=1.0, pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0,
              velocity=100, rate=0.0, atk=0.01, rel=0.04,
              reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                   lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, bpnoise=0.0, swell=0.0):
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
    return PlayerPattern(notes, dur, sus, velocity,
                         0.0, True, synthName, amp, pan, cutoff, echo, delay,
                         scaleName, atk, rel, octaveControl, baseOctave, degreesPerOctave,
                          reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, synthRate=rate, bpnoise=bpnoise, swell=swell)
def saw(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
        pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, bpnoise=0.0, swell=0.0):
    return _fox_wave('saw', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, bpnoise=bpnoise, swell=swell)

def pulse(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
          pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
          lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, bpnoise=0.0, swell=0.0):
    return _fox_wave('square', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, bpnoise=bpnoise, swell=swell)

def sine(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
         pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
                  lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, bpnoise=0.0, swell=0.0):
    return _fox_wave('sine', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, bpnoise=bpnoise, swell=swell)

def triangle(notes=0, dur=1.0, oct=5, sus=None, scale=None, amp=1.0,
             pan=0.0, cutoff=20000.0, echo=0.0, delay=0.0, velocity=100, rate=0.0, atk=0.01, rel=0.04,
        reverb='freeverb', room=0.0, mix=0.1, damp=0.5, revtime=3.0, spread=15.0,
             lpf=0.0, lpr=1.0, hpf=0.0, hpr=1.0, bpf=0.0, bpr=1.0, decay=1.0, delaytime=None, echomix=0.35, feedback=None, root=None, strum=0.0, bpnoise=0.0, swell=0.0):
    return _fox_wave('triangle', notes, dur, oct, sus, scale, amp, pan, cutoff,
                     echo, delay, velocity, rate, atk, rel,
                       reverb, room, mix, damp, revtime, spread, lpf, lpr, hpf, hpr, bpf, bpr, decay=decay, delaytime=delaytime, echomix=echomix, feedback=feedback, root=root, strum=strum, bpnoise=bpnoise, swell=swell)

def _make_named_synth(name, defaultRate):
    def synth(notes=0, **kwargs):
        kwargs.setdefault('rate', defaultRate)
        return _fox_wave(name, notes, **kwargs)
    return synth

bass = _make_named_synth('bass', 8.0)
varsaw = _make_named_synth('varsaw', 0.0)
dirt = _make_named_synth('dirt', 0.0)
jbass = _make_named_synth('jbass', 0.0)
noise = _make_named_synth('noise', 0.0)
dub = _make_named_synth('dub', 0.0)
growl = _make_named_synth('growl', 0.0)
soprano = _make_named_synth('soprano', 0.0)
crunch = _make_named_synth('crunch', 0.0)
lazer = _make_named_synth('lazer', 2.0)
rave = _make_named_synth('rave', 1.0)
bell = _make_named_synth('bell', 1.0)
gong = _make_named_synth('gong', 0.0)
marimba = _make_named_synth('marimba', 0.0)
pluck = _make_named_synth('pluck', 0.0)
twang = _make_named_synth('twang', 0.0)
donk = _make_named_synth('donk', 0.0)
squish = _make_named_synth('squish', 0.0)
charm = _make_named_synth('charm', 0.0)
spark = _make_named_synth('spark', 0.0)
blip = _make_named_synth('blip', 8.0)
scatter = _make_named_synth('scatter', 0.0)
orient = _make_named_synth('orient', 0.0)
zap = _make_named_synth('zap', 0.0)
fuzz = _make_named_synth('fuzz', 0.0)
bug = _make_named_synth('bug', 1.0)
ripple = _make_named_synth('ripple', 0.0)

def _make_core_synth(name):
    def synth(notes=0, **kwargs):
        controls = []
        specifications = (
            ('vib', 6.0 if name == 'viola' else 0.0, 0.0, 100.0),
            ('depth', 0.5, 0.0, 10.0), ('lforate', 1.0, 0.0, 100.0),
            ('lfowidth', 0.5, 0.0, 1.0), ('rq', 0.5 if name != 'prophet' else 0.4, 0.01, 3.0),
            ('fmod', 0.0, -2000.0, 2000.0), ('blur', 1.0, 0.01, 8.0))
        for key, default, minimum, maximum in specifications:
            value = float(kwargs.pop(key, default))
            if value != value or not minimum <= value <= maximum:
                raise ValueError(key + ' must be a finite scalar in the supported range')
            controls.append(value)
        kwargs.setdefault('rate', 0.04 if name == 'scratch' else
                          1.0 if name == 'swell' else 0.3 if name == 'razz' else 0.0)
        if name == 'pads':
            kwargs.setdefault('atk', 0.1)
        if name in ('sawbass', 'prophet', 'keys', 'pasha'):
            kwargs.setdefault('cutoff', {'sawbass': 1000.0, 'prophet': 6000.0,
                                        'keys': 400.0, 'pasha': 8000.0}[name])
        pattern = _fox_wave(name, notes, **kwargs)
        pattern.synthControls = tuple(controls)
        return pattern
    return synth

viola = _make_core_synth('viola')
nylon = _make_core_synth('nylon')
arpy = _make_core_synth('arpy')
dab = _make_core_synth('dab')
sawbass = _make_core_synth('sawbass')
dbass = _make_core_synth('dbass')
prophet = _make_core_synth('prophet')
pads = _make_core_synth('pads')
pasha = _make_core_synth('pasha')
ambi = _make_core_synth('ambi')
sinepad = _make_core_synth('sinepad')
keys = _make_core_synth('keys')
klank = _make_core_synth('klank')
feel = _make_core_synth('feel')
glass = _make_core_synth('glass')
soft = _make_core_synth('soft')
quin = _make_core_synth('quin')
space = _make_core_synth('space')
scratch = _make_core_synth('scratch')
creep = _make_core_synth('creep')
snick = _make_core_synth('snick')
swell = _make_core_synth('swell')
razz = _make_core_synth('razz')
sitar = _make_core_synth('sitar')
star = _make_core_synth('star')
karp = _make_core_synth('karp')

_SAMPLE_SYMBOLS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&*@^:$=!/#-%+?~\\1234'
_dexter_sample_eval_count = 0

def _sample_symbols_for_item(item):
    if isinstance(item, str) and len(item) == 1 and (
            item == ' ' or item in _SAMPLE_SYMBOLS):
        return [] if item == ' ' else [item]
    if isinstance(item, (list, tuple)) and 1 <= len(item) <= 8:
        if all(isinstance(symbol, str) and len(symbol) == 1 and
               (symbol == ' ' or symbol in _SAMPLE_SYMBOLS)
               for symbol in item):
            return [symbol for symbol in item if symbol != ' ']
    return None

def _sample_value(value, index, beat, name):
    if isinstance(value, _DexterTimeVar):
        return value.at(beat)
    if isinstance(value, (int, float)):
        return value
    values = list(value)
    if not values:
        raise ValueError(name + ' pattern cannot be empty')
    return values[index % len(values)]

def _sample_timevar_value(control, beat):
    if not control.linear or len(control.values) < 2:
        return control.at(beat)
    cycle = sum(control.durations)
    position = max(0.0, float(beat)) % cycle
    for index, duration in enumerate(control.durations):
        if position < duration or index + 1 == len(control.values):
            following = control.values[(index + 1) % len(control.values)]
            return following if position / duration >= 0.5 else control.values[index]
        position -= duration
    return control.values[-1]

def _sample_parse_nodes(text, depth=0, closing=None, position=0):
    if depth > 8:
        raise ValueError('sample-string nesting is limited to 8 levels')
    nodes = []
    pairs = {'[':']', '(':')', '{':'}', '<':'>'}
    while position < len(text):
        symbol = text[position]
        if closing is not None and symbol == closing:
            return nodes, position + 1
        if symbol in ')]}>':
            raise ValueError('unmatched closing delimiter ' + symbol + ' in sample string')
        if symbol == '|':
            pipeIndex = text.find('|', position + 1)
            if pipeIndex < 0:
                raise ValueError("missing closing '|' in FoxDot sample-index selector; use |x1|")
            selector = text[position + 1:pipeIndex]
            if (len(selector) != 2 or selector[0] not in _SAMPLE_SYMBOLS or
                    selector[1] not in '0123456789'):
                raise ValueError('sample-index selector must contain one sample symbol and one digit, like |x1|')
            nodes.append(('sample-index', (selector[0], int(selector[1]))))
            position = pipeIndex + 1
            continue
        if symbol in pairs:
            children, nextPosition = _sample_parse_nodes(
                text, depth + 1, pairs[symbol], position + 1)
            if not children:
                raise ValueError('empty ' + symbol + pairs[symbol] + ' group in sample string')
            kind = {'[':'subdivide', '(':'lace', '{':'random', '<':'angle'}[symbol]
            node = (kind, children)
            if kind == 'angle' and nodes and nodes[-1][0] == 'angle':
                nodes[-1] = ('layers', (nodes[-1][1], children))
            elif kind == 'angle' and nodes and nodes[-1][0] == 'layers':
                nodes[-1] = ('layers', nodes[-1][1] + (children,))
            else:
                nodes.append(node)
            position = nextPosition
            continue
        if not symbol.isspace() and symbol not in _SAMPLE_SYMBOLS:
            raise ValueError('unsupported sample-string character ' + repr(symbol))
        nodes.append(('symbol', symbol))
        position += 1
    if closing is not None:
        raise ValueError('missing closing delimiter ' + closing + ' in sample string')
    return nodes, position

def _sample_lcm(left, right):
    a, b = left, right
    while b:
        a, b = b, a % b
    result = left * right // a
    if result > 256:
        raise ValueError('nested sample pattern exceeds the 256-slot limit')
    return result

def _sample_nodes_width(nodes):
    width = 0
    for kind, children in nodes:
        if kind == 'angle':
            width += _sample_nodes_width(children)
        elif kind == 'layers':
            layerWidths = [_sample_nodes_width(layer) for layer in children]
            layerWidth = 1
            for value in layerWidths:
                layerWidth = _sample_lcm(layerWidth, value)
            width += layerWidth
        else:
            width += 1
        if width > 256:
            raise ValueError('nested sample pattern exceeds the 256-slot limit')
    return width

def _sample_lace_period(nodes):
    period = 1
    for kind, children in nodes:
        if kind == 'lace':
            period = _sample_lcm(period, len(children))
        elif kind == 'random':
            period = _sample_lcm(period, 8)
        if kind in ('subdivide', 'lace', 'random', 'angle'):
            period = _sample_lcm(period, _sample_lace_period(children))
        elif kind == 'layers':
            for layer in children:
                period = _sample_lcm(period, _sample_lace_period(layer))
    return period

def _sample_add_event(events, beat, symbols):
    key = round(float(beat), 10)
    if key not in events:
        events[key] = []
    events[key].extend(symbols)

def _sample_layout_sequence(nodes, cycleIndex, randomState):
    events = {}
    boundaries = {0.0}
    position = 0.0
    for node in nodes:
        width, childEvents, childBoundaries = _sample_layout_node(
            node, cycleIndex, randomState)
        for beat, symbols in childEvents.items():
            _sample_add_event(events, position + beat, symbols)
        boundaries.update(position + beat for beat in childBoundaries)
        position += width
        boundaries.add(position)
    return position, events, boundaries

def _sample_layout_node(node, cycleIndex, randomState):
    kind, children = node
    if kind == 'symbol':
        symbol = children
        return 1.0, ({0.0:[(symbol, None)]} if not symbol.isspace() else {}), {0.0, 1.0}
    if kind == 'sample-index':
        symbol, sampleIndex = children
        return 1.0, {0.0:[(symbol, sampleIndex)]}, {0.0, 1.0}
    if kind in ('subdivide', 'lace', 'random'):
        if kind == 'subdivide':
            innerWidth, innerEvents, innerBoundaries = _sample_layout_sequence(
                children, cycleIndex, randomState)
        else:
            if kind == 'lace':
                selected = children[cycleIndex % len(children)]
            else:
                randomState[0] = (1664525 * randomState[0] + 1013904223) & 0xffffffff
                selected = children[randomState[0] % len(children)]
            innerWidth, innerEvents, innerBoundaries = _sample_layout_node(
                selected, cycleIndex, randomState)
        scale = 1.0 / innerWidth
        events = {}
        for beat, symbols in innerEvents.items():
            _sample_add_event(events, beat * scale, symbols)
        boundaries = {beat * scale for beat in innerBoundaries}
        boundaries.update((0.0, 1.0))
        return 1.0, events, boundaries
    if kind == 'angle':
        return _sample_layout_sequence(children, cycleIndex, randomState)
    if kind == 'layers':
        layerWidths = [_sample_nodes_width(layer) for layer in children]
        totalWidth = 1
        for width in layerWidths:
            totalWidth = _sample_lcm(totalWidth, width)
        events = {}
        boundaries = {0.0, float(totalWidth)}
        for layer, width in zip(children, layerWidths):
            repetitions = totalWidth // width
            for repetition in range(repetitions):
                _, layerEvents, layerBoundaries = _sample_layout_sequence(
                    layer, cycleIndex, randomState)
                offset = repetition * width
                for beat, symbols in layerEvents.items():
                    _sample_add_event(events, offset + beat, symbols)
                boundaries.update(offset + beat for beat in layerBoundaries)
        return float(totalWidth), events, boundaries
    raise ValueError('unsupported nested sample pattern')

def _sample_timeline(text, parameterLengths):
    global _dexter_sample_eval_count
    nodes, _ = _sample_parse_nodes(text)
    seed = 2166136261
    for character in text:
        seed = ((seed ^ ord(character)) * 16777619) & 0xffffffff
    _dexter_sample_eval_count += 1
    seed = (seed + _dexter_sample_eval_count * 1013904223) & 0xffffffff
    baseWidth = _sample_nodes_width(nodes)
    lacePeriod = _sample_lace_period(nodes)
    if baseWidth <= 0 or lacePeriod > 256:
        raise ValueError('sample pattern has no bounded playback cycle')
    cycleLayouts = []
    unionBoundaries = set()
    randomState = [seed]
    for cycleIndex in range(lacePeriod):
        layout = _sample_layout_sequence(nodes, cycleIndex, randomState)
        width, events, boundaries = layout
        if abs(width - baseWidth) > 1e-9:
            raise ValueError('nested sample groups produced an inconsistent pattern length')
        cycleLayouts.append((events, boundaries))
        unionBoundaries.update(cycleIndex * baseWidth + beat for beat in boundaries)
    unionBoundaries.add(lacePeriod * baseWidth)
    baseBoundaries = sorted(unionBoundaries)
    slotCount = len(baseBoundaries) - 1
    if slotCount <= 0:
        raise ValueError('sample pattern contains no playback slots')
    period = slotCount
    for length in parameterLengths:
        period = _sample_lcm(period, length)
    controlRepeats = period // slotCount
    totalSlots = slotCount * controlRepeats
    if totalSlots > 256:
        raise ValueError('combined nested sample pattern exceeds 256 step slots')
    notes = []
    multipliers = []
    symbolSlots = []
    for repeat in range(controlRepeats):
        for cycleIndex, (events, _) in enumerate(cycleLayouts):
            for index in range(slotCount):
                start = baseBoundaries[index]
                end = baseBoundaries[index + 1]
                if start < cycleIndex * baseWidth or start >= (cycleIndex + 1) * baseWidth:
                    continue
                localStart = start - cycleIndex * baseWidth
                symbols = events.get(round(localStart, 10), [])
                sampleSymbols = [symbol for symbol, _ in symbols]
                notes.append(None if not sampleSymbols else
                             ord(sampleSymbols[0]) if len(sampleSymbols) == 1 else
                             tuple(ord(symbol) for symbol in sampleSymbols))
                multipliers.append(end - start)
                symbolSlots.append(tuple(symbols))
    return notes, multipliers, symbolSlots

def _sample_timeline_values(values, parameterLengths):
    if not values or len(values) > 256:
        raise ValueError('sample-symbol pattern requires 1 to 256 items')
    period = len(values)
    for length in parameterLengths:
        period = _sample_lcm(period, length)
    if period > 256:
        raise ValueError('combined sample-symbol pattern exceeds the 256-slot limit')
    notes = []
    multipliers = []
    symbolSlots = []
    for index in range(period):
        symbols = _sample_symbols_for_item(values[index % len(values)])
        if symbols is None:
            raise TypeError('play(P[...]) requires sample symbols, space-rests, or zipped sample groups')
        notes.append(None if not symbols else
                     ord(symbols[0]) if len(symbols) == 1 else
                     tuple(ord(symbol) for symbol in symbols))
        multipliers.append(1.0)
        symbolSlots.append(tuple((symbol, None) for symbol in symbols))
    return notes, multipliers, symbolSlots

def _sample_pattern(text, dur=1.0, sus=None, sample=0, rate=1.0, **controls):
    samplePatternValues = None
    if isinstance(text, _FoxDotFinitePattern):
        if any(_sample_symbols_for_item(item) is None for item in text):
            raise TypeError('play(P[...]) sample patterns require sample symbols, space-rests, or zipped sample groups')
        if any(isinstance(item, (list, tuple)) for item in text):
            samplePatternValues = list(text)
        else:
            text = ''.join(text)
    if samplePatternValues is None and (not isinstance(text, str) or not text):
        raise ValueError('sample pattern cannot be empty')
    if samplePatternValues is None and len(text) > 256:
        raise ValueError('sample pattern text is limited to 256 characters')
    sampleControl = sample if isinstance(sample, _DexterTimeVar) else None
    rateControl = rate if isinstance(rate, _DexterTimeVar) else None
    selections = ([sample.values[0]] if sampleControl is not None else
                  [sample] if isinstance(sample, (int, float)) else list(sample))
    rates = ([rate.values[0]] if rateControl is not None else
             [rate] if isinstance(rate, (int, float)) else list(rate))
    if not selections or not rates:
        raise ValueError('sample and rate patterns cannot be empty')
    if sampleControl is not None:
        if any(not float(value).is_integer() or abs(value) > 1000000
               for value in sampleControl.values):
            raise ValueError('sample var/linvar values must be whole sample indexes from -1000000 to 1000000')
    if rateControl is not None:
        if any(not 0.0625 <= abs(value) <= 8.0 for value in rateControl.values):
            raise ValueError('rate var/linvar values must have magnitude 0.0625 to 8')
        if rateControl.linear and any(value * rateControl.values[0] <= 0.0
                                      for value in rateControl.values):
            raise ValueError('rate linvar values must keep the same sign; use var() to switch direction')
    lengths = [len(selections), len(rates)]
    if not isinstance(dur, (int, float, _DexterTimeVar)):
        dur = list(dur)
        if not dur:
            raise ValueError('dur pattern cannot be empty')
        lengths.append(len(dur))
    if sus is not None and not isinstance(
            sus, (int, float, _DexterTimeVar, _FoxDotChord)):
        sus = list(sus)
        if not sus:
            raise ValueError('sus pattern cannot be empty')
        lengths.append(len(sus))
    notes, multipliers, symbolSlots = (
        _sample_timeline_values(samplePatternValues, lengths)
        if samplePatternValues is not None else
        _sample_timeline(text, lengths))
    positions = []
    symbolEvents = []
    for position, symbols in enumerate(symbolSlots):
        for symbol, sampleOverride in symbols:
            positions.append(position)
            symbolEvents.append((symbol, position, sampleOverride))
    if not any(symbols for symbols in symbolSlots):
        raise ValueError('sample pattern contains only rests')
    controls.setdefault('atk', 0.001)
    controls.setdefault('rel', 0.005)
    controls.setdefault('reverb', 'none')
    pattern = sequence(notes, step=dur, duration=sus,
                       _time_multipliers=multipliers,
                       _allow_repeated_notes=True, **controls)
    pattern.synth = 'sample'
    pattern.synthRate = 1.0
    pattern.sampleSpec = (selections, rates, sus is not None, positions,
                          sampleControl, rateControl, symbolEvents)
    return pattern

def play(notes, *args, **kwargs):
    if isinstance(notes, (str, _FoxDotFinitePattern)):
        return _sample_pattern(notes, *args, **kwargs)
    return _play_notes(notes, *args, **kwargs)

class Player:
    def __init__(self, number):
        self.number = number

    def __rshift__(self, pattern):
        global _dexter_events, _dexter_length, _dexter_slots
        global _dexter_default_touched, _dexter_scale, _dexter_root
        global _dexter_sample_controls
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
                   _time_multipliers=pattern.timeMultipliers,
                   _allow_repeated_notes=pattern.allowRepeatedNotes,
                  strum=pattern.strumControl if pattern.strumControl is not None else pattern.strumBeats)
            compiledLength = _dexter_length
            delayPattern = isinstance(pattern.delayControl, _FoxDotFinitePattern)
            if delayPattern:
                delayValues = list(pattern.delayControl)
                if len(_dexter_events) * len(delayValues) > 256:
                    raise ValueError('PDelay is limited to 256 expanded note events per player')
                compiledEvents = []
                for event in _dexter_events:
                    for delayValue in delayValues:
                        compiledEvents.append(
                            event[:6] + (event[6] + delayValue,) + event[7:])
                serializedDelayControl = None
            else:
                compiledEvents = [
                    event[:6] + (event[6] +
                                 (0.0 if pattern.delayControl is not None else pattern.delayBeats),) +
                    event[7:]
                    for event in _dexter_events
                ]
                serializedDelayControl = pattern.delayControl
        finally:
            _dexter_events, _dexter_length, _dexter_slots, _dexter_default_touched = saved[:4]
            _dexter_scale, _dexter_root = saved[4], saved[5]
        playerIndex = self.number - 1
        if not 0 <= playerIndex < 8:
            raise ValueError('player number must be 1..8')
        synthIndex = ('sine', 'triangle', 'saw', 'square', 'bass', 'varsaw',
                      'dirt', 'jbass', 'noise', 'dub', 'growl', 'soprano',
                      'crunch', 'lazer', 'rave', 'bell', 'gong', 'marimba',
                      'pluck', 'twang', 'donk', 'squish', 'charm', 'spark',
                      'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug',
                      'ripple', 'viola', 'nylon', 'arpy', 'dab', 'sawbass',
                      'dbass', 'prophet', 'pads', 'pasha', 'ambi', 'sinepad',
                      'keys', 'klank', 'feel', 'glass', 'soft', 'quin',
                      'space', 'scratch', 'creep', 'snick', 'swell',
                      'razz', 'sitar', 'star', 'karp', 'sample').index(pattern.synth)
        if pattern.synth == 'sample':
            requests = []
            adjusted = []
            assignedMidiIntervals = []
            selection, rates, gate, positions, sampleControl, rateControl, symbolEvents = pattern.sampleSpec
            symbolsByPosition = {}
            for symbolEvent in symbolEvents:
                symbolsByPosition.setdefault(symbolEvent[1], []).append(symbolEvent)
            for event in compiledEvents:
                sourcePosition = event[8]
                chordIndex = event[9]
                symbol, position, sampleOverride = \
                    symbolsByPosition[sourcePosition][chordIndex]
                symbol = ord(symbol)
                sampleIndex = (sampleOverride if sampleOverride is not None else
                               _sample_timevar_value(sampleControl, event[0])
                               if sampleControl is not None else
                               _sample_value(selection, position, event[0], 'sample'))
                playbackRate = float(_sample_value(rates, position, event[0], 'rate'))
                if int(sampleIndex) != sampleIndex or not -1000000 <= sampleIndex <= 1000000:
                    raise ValueError('sample selection must be an integer from -1000000 to 1000000')
                if playbackRate != playbackRate or not 0.0625 <= abs(playbackRate) <= 8.0:
                    raise ValueError('sample rate magnitude must be 0.0625 to 8; negative plays backwards')
                requests.append((symbol, int(sampleIndex), playbackRate, int(gate)))
                note = list(event)
                baseMidiNote = {120: 36, 88: 36, 111: 38, 79: 38,
                                45: 42, 61: 46, 42: 39}.get(symbol, symbol)
                startBeat = event[0] + event[6]
                endBeat = startBeat + event[1]
                candidates = list(range(baseMidiNote, 128)) + list(range(baseMidiNote - 1, -1, -1))
                midiNote = None
                for candidate in candidates:
                    if all(candidate != oldNote or startBeat >= oldEnd - 1e-9 or
                           oldStart >= endBeat - 1e-9
                           for oldNote, oldStart, oldEnd in assignedMidiIntervals):
                        midiNote = candidate
                        break
                if midiNote is None:
                    raise ValueError('sample layers exceed the available MIDI pitch range')
                assignedMidiIntervals.append((midiNote, startBeat, endBeat))
                note[2] = midiNote
                adjusted.append(tuple(note[:7]))
            compiledEvents = adjusted
            _dexter_sample_events[playerIndex] = requests
            _dexter_sample_controls[playerIndex] = (
                sampleControl.wire() if sampleControl is not None else '-',
                rateControl.wire() if rateControl is not None else '-')
        _dexter_synth_controls[playerIndex] = pattern.synthControls
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
                                         pattern.echoMix, pattern.echoMixControl, pattern.feedback, pattern.feedbackControl, serializedDelayControl, pattern.synthRate,
                                         pattern.bpnoise, pattern.bpnoiseControl, pattern.swell, pattern.swellControl,
                                         _dexter_swing_seconds)
        if playerIndex == 0:
            _dexter_default_touched = False
        return self

    def stop(self):
        _dexter_updates[self.number - 1] = (True, _dexter_bpm, 1.0, [], 0, 1.0,
                                             0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-', 0.0, 0.0, None, 0.0, None, _dexter_swing_seconds)

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
            << "'_dexter_scale': (0, 2, 4, 5, 7, 9, 11), '_dexter_root': 60, '_dexter_mutes': {}, "
            << "'_dexter_swing_seconds': 0.0, "
           << "'_dexter_shuffle_seed': int(time.monotonic_ns() & 0xffffffff)})\n"
           << "_dexter_scope['__builtins__'] = {name: getattr(builtins, name) for name in "
           << "('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter', 'float', "
           << "'int', 'isinstance', 'len', 'list', 'map', 'max', 'min', 'range', 'ord', 'chr', "
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
              << "            _dexter_scope['_dexter_updates'][0] = (False, _dexter_scope['_dexter_bpm'], _dexter_scope['_dexter_length'], list(_dexter_scope['_dexter_events']), 0, 1.0, 0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-', 0.0, 0.0, None, 0.0, None) + (_dexter_scope['_dexter_swing_seconds'],)\n"
            << "        _updates = _dexter_scope['_dexter_updates']\n"
            << "        _mutes = _dexter_scope['_dexter_mutes']\n"
            << "        _f.write('OK\\t{}\\t{}\\n'.format(len(_updates), len(_mutes)))\n"
            << "        for _player in sorted(_updates):\n"
              << "            _stopped, _bpm, _length, _events, _synth, _amp, _pan, _cutoff, _echo, _delay, _mouse_pan, _mouse_cutoff, _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control, _reverb, _mix, _room, _damp, _revtime, _spread, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, _lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control, _decay, _decay_control, _echo_mix, _echo_mix_control, _feedback, _feedback_control, _note_delay_control, _synth_rate, _bpnoise, _bpnoise_control, _swell, _swell_control, _swing_seconds = _updates[_player]\n"
             << "            _p_values = (_player, int(_stopped), _bpm, _length, len(_events), _synth, _amp, _pan, _cutoff, _echo, _delay, int(_mouse_pan), int(_mouse_cutoff), _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control, _synth_rate, _swing_seconds)\n"
             << "            _f.write('P' + chr(9) + chr(9).join(map(str, _p_values)) + chr(10))\n"
            << "            _f.write('R\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _reverb, _mix, _room, _damp, _revtime, _spread))\n"
             << "            _f.write('E\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _decay, _decay_control.wire() if _decay_control is not None else '-', _echo_mix, _echo_mix_control, _feedback, _feedback_control if _feedback_control is not None else '-', _note_delay_control.wire() if hasattr(_note_delay_control, 'wire') else (_note_delay_control if _note_delay_control is not None else '-')))\n"
              << "            _controls = (_lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control)\n"
             << "            _f.write('F\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, *[control.wire() if control is not None else '-' for control in _controls]))\n"
            << "            _modulation_values = (_player, _bpnoise, _swell, _bpnoise_control.wire() if _bpnoise_control is not None else '-', _swell_control.wire() if _swell_control is not None else '-')\n"
            << "            _f.write('G' + chr(9) + chr(9).join(map(str, _modulation_values)) + chr(10))\n"
            << "            _synth_controls = _dexter_scope['_dexter_synth_controls'].get(_player, (0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0))\n"
            << "            _f.write('S' + chr(9) + str(_player) + chr(9) + chr(9).join(map(str, _synth_controls)) + chr(10))\n"
             << "            if _synth == 57:\n"
             << "                _sample_control, _rate_control = _dexter_scope['_dexter_sample_controls'].get(_player, ('-', '-'))\n"
             << "                _f.write('Q' + chr(9) + str(_player) + chr(9) + _sample_control + chr(9) + _rate_control + chr(10))\n"
            << "            for _event_index, _e in enumerate(_events):\n"
            << "                _f.write('N\\t{}\t{}\t{}\t{}\t{}\t{}\\t{}\\n'.format(*_e))\n"
            << "                if _synth == 57:\n"
            << "                    _f.write('B\\t{}\\t{}\\t{}\\t{}\\n'.format(*_dexter_scope['_dexter_sample_events'][_player][_event_index]))\n"
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
    // Python opens text files with CRLF on Windows; std::getline removes LF only.
    if (!header.empty() && header.back() == '\r')
        header.pop_back();
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
              update.echoControlToken >> update.delayTimeControlToken >> update.synthRate >>
              update.swingSeconds) ||
            record != 'P' || update.playerIndex < 0 || update.playerIndex >= 8 ||
            stopped < 0 || stopped > 1 ||
            seenPlayers[static_cast<std::size_t>(update.playerIndex)] || eventCount > 256 ||
            !std::isfinite(update.bpm) || update.bpm < 0.001 ||
            !std::isfinite(update.lengthBeats) || update.lengthBeats <= 0.0 ||
              update.lengthBeats > 4096.0 || update.synthIndex < 0 || update.synthIndex > 57 ||
             !std::isfinite(update.synthRate) || update.synthRate < 0.0 ||
             update.synthRate > 10.0 ||
              (update.synthIndex == 5 && update.synthRate > 1.0) ||
              !std::isfinite(update.swingSeconds) ||
              std::abs(update.swingSeconds * update.bpm / 60.0) >= 0.49 ||
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
        char modulationRecord = 0;
        int modulationPlayer = -1;
        if (!(input >> modulationRecord >> modulationPlayer >> update.bpnoise >>
              update.swell >> update.bpnoiseControlToken >> update.swellControlToken) ||
            modulationRecord != 'G' || modulationPlayer != update.playerIndex ||
            !std::isfinite(update.bpnoise) || update.bpnoise < 0.0 ||
            update.bpnoise > 10.0 || !std::isfinite(update.swell) ||
            update.swell < 0.0 || update.swell > 4.0) {
            error = "Python returned malformed filter-modulation controls.";
            return false;
        }
        char synthRecord = 0;
        int synthPlayer = -1;
        if (!(input >> synthRecord >> synthPlayer) ||
            synthRecord != 'S' || synthPlayer != update.playerIndex) {
            error = "Python returned malformed synth controls.";
            return false;
        }
        constexpr std::array<double, 7> minimum{0, 0, 0, 0, 0.01, -2000, 0.01};
        constexpr std::array<double, 7> maximum{100, 10, 100, 1, 3, 2000, 8};
        for (std::size_t index = 0; index < update.synthControls.size(); ++index) {
            auto& value = update.synthControls[index];
            if (!(input >> value) || !std::isfinite(value) ||
                value < minimum[index] || value > maximum[index]) {
                error = "Python returned an invalid synth control.";
                return false;
            }
        }
        if (update.synthIndex == 57) {
            char controlRecord = 0;
            int controlPlayer = -1;
            auto validControlToken = [](const std::string& token) {
                if (token == "-") return true;
                if (token.size() > 256 || token.size() < 5 ||
                    (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                    return false;
                const auto delimiter = token.find(':', 2);
                return delimiter != std::string::npos && delimiter > 2 &&
                       delimiter + 1 < token.size() &&
                       token.find(':', delimiter + 1) == std::string::npos;
            };
            if (!(input >> controlRecord >> controlPlayer >> update.sampleControlToken >>
                  update.sampleRateControlToken) ||
                controlRecord != 'Q' || controlPlayer != update.playerIndex ||
                !validControlToken(update.sampleControlToken) ||
                !validControlToken(update.sampleRateControlToken)) {
                error = "Python returned malformed sample time-var controls.";
                return false;
            }
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
                event.durationBeats > std::min(4096.0, update.lengthBeats * 8.0) + 1e-9 ||
                event.midiNote < 0 || event.midiNote > 127 ||
                event.velocity < 1 || event.velocity > 127 ||
                !std::isfinite(event.attackSeconds) || event.attackSeconds < 0.0 ||
                event.attackSeconds > 3.0 || !std::isfinite(event.releaseSeconds) ||
                event.releaseSeconds < 0.0 || event.releaseSeconds > 3.0) {
                error = "Python returned a malformed note event.";
                return false;
            }
            if (update.synthIndex == 57) {
                char bufferRecord = 0;
                int gate = 0;
                if (!(input >> bufferRecord >> event.sampleSymbol >> event.sampleIndex >>
                      event.sampleRate >> gate) || bufferRecord != 'B' ||
                    event.sampleSymbol < 33 || event.sampleSymbol > 126 ||
                    event.sampleIndex < -1000000 || event.sampleIndex > 1000000 ||
                    !std::isfinite(event.sampleRate) ||
                    std::abs(event.sampleRate) < 0.0625 || std::abs(event.sampleRate) > 8.0 ||
                    (gate != 0 && gate != 1)) {
                    error = "Python returned a malformed sample event.";
                    return false;
                }
                event.sampleGate = gate != 0;
            }
            update.events.push_back(event);
        }
        std::sort(update.events.begin(), update.events.end(),
                  [](const PythonNoteEvent& a, const PythonNoteEvent& b) {
                      return a.beat < b.beat;
                  });
        std::vector<std::pair<double, int>> boundaries;
        boundaries.reserve(update.events.size() * 4);
        int activeNotes = 0;
        for (const auto& event : update.events) {
            double startBeat = std::fmod(event.beat + event.delayBeats, update.lengthBeats);
            if (startBeat < 0.0) startBeat += update.lengthBeats;
            const double fullCycles = std::floor(event.durationBeats / update.lengthBeats);
            if (fullCycles > 8.0 || activeNotes + static_cast<int>(fullCycles) > 8) {
                std::ostringstream message;
                message << "Player " << (update.playerIndex + 1)
                        << " exceeds 8-note polyphony across overlapping sustains.";
                error = message.str();
                return false;
            }
            activeNotes += static_cast<int>(fullCycles);
            const double remainder =
                event.durationBeats - fullCycles * update.lengthBeats;
            if (remainder <= 1e-9)
                continue;
            const double endBeat = startBeat + remainder;
            if (endBeat <= update.lengthBeats) {
                boundaries.emplace_back(startBeat, 1);
                boundaries.emplace_back(endBeat, -1);
            } else {
                boundaries.emplace_back(0.0, 1);
                boundaries.emplace_back(endBeat - update.lengthBeats, -1);
                boundaries.emplace_back(startBeat, 1);
                boundaries.emplace_back(update.lengthBeats, -1);
            }
        }
        std::sort(boundaries.begin(), boundaries.end(),
                  [](const auto& a, const auto& b) {
                      if (a.first != b.first) return a.first < b.first;
                      return a.second < b.second;
                  });
        for (const auto& boundary : boundaries) {
            activeNotes += boundary.second;
            if (activeNotes > 8) {
                std::ostringstream message;
                message << "Player " << (update.playerIndex + 1)
                        << " exceeds 8-note polyphony near beat " << boundary.first << '.';
                error = message.str();
                return false;
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
    LifecycleTrace trace("Python initialize including lock", this);
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
