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
constexpr std::size_t kEmbeddedSynthCount = 58;

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

std::string pythonStringTuple(const std::vector<std::string>& values) {
    std::ostringstream result;
    result << "(";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0)
            result << ", ";
        result << pythonLiteral(values[index]);
    }
    if (values.size() == 1)
        result << ",";
    result << ")";
    return result.str();
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
                                    const std::string& reportPath,
                                    const std::vector<std::string>& userSynthNames) {
    static constexpr char compatibilityChecker[] = R"PY(
import ast
import re

_USER_SYNTH_NAMES = globals().get('_USER_SYNTH_NAMES', ())
_COMPAT_CALLS = {
    'abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
    'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
    'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum',
    'tuple', 'zip', 'ValueError', 'TypeError', 'ZeroDivisionError',
    'tempo', 'scale', 'reverse', 'rotate', 'repeat', 'palindrome', 'stutter',
    'interleave', 'transpose', 'euclid', 'PAlt', 'PStretch', 'PStutter',
    'PShuf', 'PJoin', 'PBeat', 'PDur', 'PDelay', 'PStrum', 'PQuicken', 'PRhythm',
    'PZip', 'PZip2', 'PRand', 'PWhite', 'PWalk', 'PChain', 'PDelta',
    'PSquare',
    'play', 'sequence', 'saw', 'pulse', 'sine', 'triangle', 'bass', 'varsaw',
    'dirt', 'jbass', 'noise', 'dub', 'growl', 'soprano', 'crunch', 'lazer', 'rave',
    'bell', 'gong', 'marimba', 'pluck', 'twang', 'donk', 'squish',
    'charm', 'spark', 'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug',
    'ripple', 'viola', 'nylon', 'arpy', 'dab', 'sawbass', 'dbass',
    'prophet', 'pads', 'pasha', 'ambi', 'sinepad', 'keys', 'klank',
    'feel', 'glass', 'soft', 'quin', 'space', 'scratch', 'creep',
    'snick', 'swell', 'razz', 'sitar', 'star', 'karp',
    'var', 'linvar', 'Clock', 'Scale', 'Root', 'PGroup',
    'Player', 'PlayerPattern', 'Group', 'Master', 'MouseX', 'MouseY', 'userSynth', 'DiskIn', 'VDiskIn', 'DiskOut',
    '__build_class__'
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
_USER_SYNTH_KEYWORDS = {
    'notes', 'dur', 'oct', 'sus', 'scale', 'root', 'amp', 'pan', 'cutoff',
    'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'velocity',
    'rate', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime',
    'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell',
    'strum', 'vib', 'depth', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur'
}
_COMPAT_SEQUENCE_KEYWORDS = {
    'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'synth',
    'amp', 'pan', 'cutoff', 'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell', 'root', 'strum', 'rate'
}
_COMPAT_NOTE_PLAY_KEYWORDS = {'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'strum'}
_COMPAT_SAMPLE_PLAY_KEYWORDS = (_COMPAT_SEQUENCE_KEYWORDS - {'notes', 'step', 'duration', 'degrees', 'synth', 'root', 'strum'}) | {'dur', 'sus', 'sample', 'start', 'end', 'loop', 'effect', 'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur'}
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
_COMPAT_GROUP_CONTROLS = {
    'amp', 'pan', 'cutoff', 'echo', 'delaytime', 'atk', 'rel', 'reverb',
    'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf',
    'hpr', 'bpf', 'bpr', 'decay', 'echomix', 'feedback', 'bpnoise', 'swell',
    'degree', 'dur', 'sus', 'oct', 'scale', 'root'
}
_COMPAT_GROUP_METHODS = {
    'add', 'stop', 'mute', 'unmute', 'solo', 'only',
    'every', 'often', 'sometimes', 'rarely'
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
        self.group_names = {'p_all'}
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
            if (isinstance(node, ast.Assign) and isinstance(node.value, ast.Call) and
                    isinstance(node.value.func, ast.Name) and
                    node.value.func.id in {'Group', 'Master'}):
                for target in node.targets:
                    if isinstance(target, ast.Name):
                        self.group_names.add(target.id)

    def _is_group_expression(self, node):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            return node.func.id in {'Group', 'Master'}
        return isinstance(node, ast.Name) and node.id in self.group_names

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
        if name == 'Group':
            playerNames = [
                argument.id for argument in node.args
                if isinstance(argument, ast.Name)]
            validPlayers = (
                0 <= len(node.args) <= 8 and not node.keywords and
                len(playerNames) == len(node.args) and
                len(set(playerNames)) == len(playerNames) and
                all(re.fullmatch(r'p[1-8]', playerName)
                    for playerName in playerNames))
            if not validPlayers:
                self.add(node.lineno,
                         'Group accepts zero to eight distinct players named p1 through p8.')
        if name == 'Master' and (node.args or node.keywords):
            self.add(node.lineno, 'Master() takes no arguments in the embedded API.')
        if (isinstance(node.func, ast.Attribute) and
                self._is_group_expression(node.func.value)):
            method = node.func.attr
            if method not in _COMPAT_GROUP_METHODS and method not in _COMPAT_GROUP_CONTROLS:
                self.add(node.lineno, 'Group supports shared sound/effect and pitch/timing controls, add(), stop(), mute(), unmute(), solo(), only(), and bounded player scheduling.')
            elif method in _COMPAT_GROUP_CONTROLS:
                self.add(node.lineno, 'Group controls are assigned as attributes, e.g. group.amp = 0.5.')
            elif method in {'stop', 'mute', 'unmute', 'only'} and (
                    node.args or node.keywords):
                self.add(node.lineno, 'Group.' + method + '() takes no arguments.')
            elif method == 'solo' and (
                    len(node.args) > 1 or node.keywords or
                    (node.args and not isinstance(node.args[0], ast.Constant)) or
                    (node.args and not isinstance(node.args[0].value, bool))):
                self.add(node.lineno, 'Group.solo accepts only an optional literal True or False.')
            elif method == 'add' and (
                    len(node.args) != 1 or node.keywords or
                    not isinstance(node.args[0], ast.Name) or
                    not re.fullmatch(r'p[1-8]', node.args[0].id)):
                self.add(node.lineno, 'Group.add accepts one player name from p1 through p8.')
        if name == 'userSynth':
            if not node.args or not isinstance(node.args[0], ast.Constant) or not isinstance(node.args[0].value, str):
                self.add(node.lineno, 'userSynth requires the literal name of a loaded .scsyndef, for example userSynth(\"MyPluck\", notes).')
            elif node.args[0].value not in _USER_SYNTH_NAMES:
                self.add(node.lineno, 'userSynth name is not present in Portable/SynthDefs: ' + node.args[0].value)
            for keyword in node.keywords:
                if keyword.arg is None:
                    self.add(keyword.lineno or node.lineno, 'Dynamic keyword expansion cannot be checked; use explicit userSynth controls.')
                elif keyword.arg not in _USER_SYNTH_KEYWORDS:
                    self.add(keyword.lineno or node.lineno, "Unsupported userSynth control '" + keyword.arg + "'.")
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
        if (isinstance(node.func, ast.Attribute) and
                isinstance(node.func.value, ast.Name) and
                node.func.value.id in {'p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'} and
                node.func.attr == 'follow'):
            if (len(node.args) != 1 or node.keywords or
                    not isinstance(node.args[0], ast.Name) or
                    not re.fullmatch(r'p[1-8]', node.args[0].id)):
                self.add(node.lineno, 'Player.follow requires one player name, for example p2.follow(p1).')
            elif node.args[0].id == node.func.value.id:
                self.add(node.lineno, 'A player cannot follow itself.')
        if (isinstance(node.func, ast.Attribute) and
                node.func.attr in {'every', 'often', 'sometimes', 'rarely'} and
                ((isinstance(node.func.value, ast.Name) and
                  node.func.value.id in {
                      'p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'}) or
                 self._is_group_expression(node.func.value))):
            validCount = (2 <= len(node.args) <= 3 if node.func.attr == 'every'
                          else 1 <= len(node.args) <= 2)
            commandIndex = 1 if node.func.attr == 'every' else 0
            validCommand = (len(node.args) > commandIndex and
                            isinstance(node.args[commandIndex], ast.Constant) and
                            node.args[commandIndex].value in
                            {'reverse', 'rotate', 'shuffle', 'stutter'})
            keywordNodes = {
                keyword.arg: keyword.value for keyword in node.keywords
                if keyword.arg is not None
            }
            supportedKeywords = all(
                keyword.arg in {'cycle', 'ident'}
                for keyword in node.keywords)
            cycleNode = keywordNodes.get('cycle')
            cycleValue = (cycleNode.value if
                          isinstance(cycleNode, ast.Constant) else None)
            validCycle = (
                cycleNode is None or
                (isinstance(cycleValue, (int, float)) and
                 not isinstance(cycleValue, bool) and
                 0.05 <= cycleValue <= 4096.0))
            validInterval = True
            if node.func.attr == 'every':
                validInterval = (
                    node.args and isinstance(node.args[0], ast.Constant) and
                    isinstance(node.args[0].value, (int, float)) and
                    not isinstance(node.args[0].value, bool) and
                    (0.0 <= node.args[0].value < cycleValue
                     if cycleNode is not None and validCycle else
                     0.05 <= node.args[0].value <= 4096.0))
            identNode = keywordNodes.get('ident')
            identValue = (identNode.value if
                          isinstance(identNode, ast.Constant) else None)
            validIdent = (
                identNode is None or
                (isinstance(identValue, (str, int)) and
                 not isinstance(identValue, bool) and
                 0 < len(str(identValue)) <= 64 and
                 all(character.isalnum() or character in '_-'
                     for character in str(identValue))))
            command = (node.args[commandIndex].value if validCommand else None)
            hasAmount = len(node.args) == commandIndex + 2
            amount = node.args[-1].value if (
                hasAmount and isinstance(node.args[-1], ast.Constant)) else None
            validAmount = (not hasAmount or
                           (isinstance(amount, int) and
                            not isinstance(amount, bool) and
                            ((command == 'rotate' and -256 <= amount <= 256) or
                             (command == 'stutter' and 1 <= amount <= 16))))
            if not supportedKeywords:
                self.add(node.lineno, 'Embedded player scheduling accepts only constant cycle= and ident= keywords.')
            if not validCount or not validCommand or not validInterval or not validAmount or not validCycle or not validIdent:
                self.add(node.lineno, "Use bounded transforms such as p1.every(8, 'reverse'), p1.every(1, 'stutter', 3, cycle=8, ident='fill'), p1.often('shuffle'), or p1.rarely('stutter', 2).")
        if name in {'PRand', 'PWhite'}:
            parameters = ('start', 'stop') if name == 'PRand' else ('lo', 'hi')
            allowed = set(parameters) | {'length', 'seed'}
            if len(node.args) > 2:
                self.add(node.lineno, name + ' accepts at most two positional bounds/patterns.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(keyword.lineno or node.lineno,
                             name + ' accepts only ' +
                             ', '.join(parameters + ('length', 'seed')) + '.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             name + ' received the same argument more than once.')
                elif keyword.arg in parameters:
                    positional.add(keyword.arg)
            if name == 'PRand' and not positional.intersection({'start'}):
                self.add(node.lineno, 'PRand requires a start value or source pattern.')
        if name == 'PWalk':
            parameters = ('max', 'step', 'start', 'length', 'seed')
            allowed = set(parameters)
            if len(node.args) > len(parameters):
                self.add(node.lineno, 'PWalk accepts max, step, start, length, and seed.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(keyword.lineno or node.lineno,
                             'PWalk accepts only max, step, start, length, and seed.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             'PWalk received the same argument more than once.')
                else:
                    positional.add(keyword.arg)
        if name == 'PChain':
            parameters = ('mapping', 'length', 'seed')
            allowed = set(parameters)
            if len(node.args) > len(parameters):
                self.add(node.lineno, 'PChain accepts mapping, length, and seed.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(node.lineno, 'PChain accepts only mapping, length, and seed.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             'PChain received the same argument more than once.')
                else:
                    positional.add(keyword.arg)
            if not positional.intersection({'mapping'}):
                self.add(node.lineno, 'PChain requires a transition mapping.')
        if name == 'PDelta':
            parameters = ('deltas', 'start', 'length')
            allowed = set(parameters)
            if len(node.args) > len(parameters):
                self.add(node.lineno, 'PDelta accepts deltas, start, and length.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(node.lineno, 'PDelta accepts only deltas, start, and length.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             'PDelta received the same argument more than once.')
                else:
                    positional.add(keyword.arg)
            if not positional.intersection({'deltas'}):
                self.add(node.lineno, 'PDelta requires a finite delta pattern.')
        if name == 'PSquare':
            parameters = ('length',)
            allowed = set(parameters)
            if len(node.args) > len(parameters):
                self.add(node.lineno, 'PSquare accepts only a length argument.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(node.lineno, 'PSquare accepts only a length argument.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             'PSquare received the same argument more than once.')
                else:
                    positional.add(keyword.arg)
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
        if name == 'play':
            for keyword in node.keywords:
                if keyword.arg != 'effect':
                    continue
                if not isinstance(keyword.value, ast.Constant) or not isinstance(keyword.value.value, str):
                    self.add(keyword.lineno or node.lineno,
                             "play(effect=...) requires a literal 'AirwindowsTapeDelay2' or 'AirwindowsEchojam' name.")
                elif keyword.value.value not in {'AirwindowsTapeDelay2', 'AirwindowsEchojam'}:
                    self.add(keyword.lineno or node.lineno,
                             'play(effect=...) supports AirwindowsTapeDelay2 or AirwindowsEchojam.')
                elif keyword.value.value not in _USER_SYNTH_NAMES:
                    self.add(keyword.lineno or node.lineno,
                             'play(effect=...) requires the matching Airwindows SynthDef in Portable/SynthDefs.')
        if name in _COMPAT_WAVES and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            self.add(node.lineno, 'Use play(\"...\") for sample strings, not a pitched synth call.')
        if name == 'play' and node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            try:
                _compat_validate_play_string(node.args[0].value)
            except ValueError as error:
                self.add(node.lineno, 'Unsupported FoxDot play string: ' + str(error))
        self.generic_visit(node)

    def visit_Attribute(self, node):
        if self._is_group_expression(node.value):
            if (node.attr not in _COMPAT_GROUP_CONTROLS and
                    node.attr not in _COMPAT_GROUP_METHODS):
                self.add(node.lineno, 'Unsupported Group attribute "' +
                         node.attr + '".')
        if isinstance(node.value, ast.Name) and node.value.id in {'DiskIn', 'VDiskIn', 'DiskOut'}:
            allowed = {'record', 'stop'} if node.value.id == 'DiskOut' else {'play', 'stop'}
            if node.attr not in allowed:
                self.add(node.lineno, node.value.id + ' supports only ' + ', '.join(sorted(allowed)) + ' in this VST extension; sclang .ar graph building is not embedded.')
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
            self.add(node.lineno, 'Root.default only supports .set(root), with fixed notes or bounded var/linvar values.')
        if isinstance(node.value, ast.Name) and re.fullmatch(r'p[0-9]+', node.value.id):
            if not re.fullmatch(r'p[1-8]', node.value.id):
                self.add(node.lineno, 'Use one of the supported player names p1 through p8.')
            elif node.attr not in {'stop', 'mute', 'unmute', 'follow', 'degree', 'every', 'often', 'sometimes', 'rarely'}:
                self.add(node.lineno, "Player " + node.value.id + ' supports .stop(), .mute(), .unmute(), .follow(), degree references, and bounded .every()/.often()/.sometimes()/.rarely() transforms.')
        self.generic_visit(node)

    def visit_Assign(self, node):
        for target in node.targets:
            if (isinstance(target, ast.Attribute) and
                    self._is_group_expression(target.value) and
                    target.attr not in _COMPAT_GROUP_CONTROLS):
                self.add(target.lineno, 'Group assignments support shared sound/effect and pitch/timing controls.')
            if (isinstance(target, ast.Attribute) and
                    isinstance(target.value, ast.Name) and
                    re.fullmatch(r'p[0-9]+', target.value.id)):
                if not re.fullmatch(r'p[1-8]', target.value.id):
                    self.add(target.lineno, 'Use one of the supported player names p1 through p8.')
                elif (target.attr != 'degree' or
                      not isinstance(node.value, ast.Attribute) or
                      not isinstance(node.value.value, ast.Name) or
                      not re.fullmatch(r'p[1-8]', node.value.value.id) or
                      node.value.attr != 'degree'):
                    self.add(target.lineno, 'Player degree assignment supports only p2.degree = p1.degree-style player references.')
                elif target.value.id == node.value.value.id:
                    self.add(target.lineno, 'A player degree cannot reference itself.')
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
            lines.append('Supported synth calls: ' + ', '.join(sorted(_COMPAT_WAVES)) + '. Loaded external SynthDefs: ' + ', '.join(_USER_SYNTH_NAMES) + '. WAV play strings support symbols, rests, [], (), {}, adjacent <> layers, and per-hit sample selectors like |x1|.')
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
           << "_USER_SYNTH_NAMES = " << pythonStringTuple(userSynthNames) << "\n"
           << "exec(compile(" << pythonLiteral(compatibilityChecker)
           << ", '<Dexter compatibility checker>', 'exec'), globals(), globals())\n";
    return script.str();
}

std::string makeEvaluationScript(const std::string& sourcePath,
                                const std::string& resultPath,
                                const std::string& namespaceName,
                                const std::vector<std::string>& userSynthNames) {
    static constexpr char api[] = R"PY(
_dexter_synth_controls = {}
_dexter_sample_events = {}
_dexter_sample_base_events = {}
_dexter_sample_controls = {}
_dexter_player_effect_indices = {}
_dexter_effect_control_variables = {}
_dexter_shuffle_counter = 0
_dexter_random_counter = 0
_dexter_swing_seconds = 0.0
_NEW_SYNTHS = ('viola', 'nylon', 'arpy', 'dab', 'sawbass', 'dbass',
               'prophet', 'pads', 'pasha', 'ambi', 'sinepad', 'keys',
               'klank', 'feel', 'glass', 'soft', 'quin', 'space',
               'scratch', 'creep', 'snick', 'swell', 'razz', 'sitar',
               'star', 'karp')
_DEXTER_BUILTIN_SYNTHS = (
    'sine', 'triangle', 'saw', 'square', 'bass', 'varsaw', 'dirt', 'jbass',
    'noise', 'dub', 'growl', 'soprano', 'crunch', 'lazer', 'rave', 'bell',
    'gong', 'marimba', 'pluck', 'twang', 'donk', 'squish', 'charm', 'spark',
    'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug', 'ripple', 'viola',
    'nylon', 'arpy', 'dab', 'sawbass', 'dbass', 'prophet', 'pads', 'pasha',
    'ambi', 'sinepad', 'keys', 'klank', 'feel', 'glass', 'soft', 'quin',
    'space', 'scratch', 'creep', 'snick', 'swell', 'razz', 'sitar', 'star',
    'karp', 'sample')
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
    global _dexter_scale, _dexter_root, _dexter_root_control
    root = int(root)
    if not 0 <= root <= 127:
        raise ValueError('scale root must be a MIDI note from 0 to 127')
    _dexter_scale = _scale_intervals(name)
    _dexter_root = root
    _dexter_root_control = None

class _FoxDotScaleDefault:
    def set(self, name, root=None):
        global _dexter_scale, _dexter_root, _dexter_root_control
        if root is None:
            _dexter_scale = _scale_intervals(name)
        else:
            _dexter_scale = _scale_intervals(name)
            _dexter_root = _root_midi(root)
            _dexter_root_control = None

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
        global _dexter_root, _dexter_root_control
        if value is None:
            raise ValueError('Root.default.set requires a pitch class, MIDI note, or bounded var/linvar')
        _dexter_root, _dexter_root_control = _normalize_root_control(value)

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

    def mirror(self):
        def mirror_item(item, depth):
            if depth > 16:
                raise ValueError('mirror is limited to 16 nested pattern levels')
            if isinstance(item, _FoxDotFinitePattern):
                return _FoxDotFinitePattern([
                    mirror_item(child, depth + 1) for child in reversed(item)])
            if isinstance(item, tuple):
                return tuple(mirror_item(child, depth + 1)
                             for child in reversed(item))
            if isinstance(item, list):
                return [mirror_item(child, depth + 1)
                        for child in reversed(item)]
            return item
        return _FoxDotFinitePattern([
            mirror_item(item, 0) for item in reversed(self)])

    def accum(self, n=None):
        counts = ([len(self)] if n is None else
                  list(n) if isinstance(n, (list, tuple, _FoxDotFinitePattern))
                  else [n])
        if not counts:
            raise ValueError('accum count pattern cannot be empty')
        if len(counts) > 256:
            raise ValueError('accum count patterns are limited to 256 items')
        result = []
        for count in counts:
            if isinstance(count, bool) or not isinstance(count, int):
                raise TypeError('accum lengths must be whole numbers from 0 to 256')
            if not 0 <= count <= 256:
                raise ValueError('accum lengths must be whole numbers from 0 to 256')
            values = [0]
            if count > 1 and not self:
                raise ValueError('accum needs a non-empty source pattern')
            total = 0
            for index in range(count - 1):
                item = self[index % len(self)]
                if not _finite_note_number(item):
                    raise TypeError('accum supports finite numeric note patterns only')
                total += item
                if not _finite_note_number(total):
                    raise ValueError('accum results must remain finite numbers')
                values.append(total)
            if len(result) + len(values) > 256:
                raise ValueError('accum output is limited to 256 items')
            result.extend(values)
        return _FoxDotFinitePattern(result)

    def swap(self, n=2):
        sizes = (list(n) if isinstance(n, (list, tuple, _FoxDotFinitePattern))
                 else [n])
        if not sizes:
            raise ValueError('swap block-size pattern cannot be empty')
        if len(sizes) > 256:
            raise ValueError('swap block-size patterns are limited to 256 items')
        result = []
        for size in sizes:
            if isinstance(size, bool) or not isinstance(size, int):
                raise TypeError('swap block sizes must be whole numbers from 1 to 256')
            if not 1 <= size <= 256:
                raise ValueError('swap block sizes must be whole numbers from 1 to 256')
            swapped = []
            for start in range(0, len(self), size):
                swapped.extend(reversed(self[start:start + size]))
            if len(result) + len(swapped) > 256:
                raise ValueError('swap output is limited to 256 items')
            result.extend(swapped)
        return _FoxDotFinitePattern(result)

    def repeat(self, count=2):
        count = int(count)
        if count < 1 or len(self) * count > 256:
            raise ValueError('repeat is limited to 256 expanded items and requires a positive count')
        return _FoxDotFinitePattern(list(self) * count)

    def stretch(self, size):
        sizes = (list(size) if isinstance(size, (list, tuple, _FoxDotFinitePattern))
                 else [size])
        if not sizes:
            raise ValueError('stretch target-size pattern cannot be empty')
        if len(sizes) > 256:
            raise ValueError('stretch target-size patterns are limited to 256 items')
        result = []
        for target_size in sizes:
            if isinstance(target_size, bool) or not isinstance(target_size, int):
                raise TypeError('stretch sizes must be whole numbers from 0 to 256')
            if not 0 <= target_size <= 256:
                raise ValueError('stretch sizes must be whole numbers from 0 to 256')
            if len(result) + target_size > 256:
                raise ValueError('stretch output is limited to 256 items')
            result.extend(PStretch(self, target_size))
        return _FoxDotFinitePattern(result)

    def loop(self, count):
        counts = (list(count) if isinstance(count, (list, tuple, _FoxDotFinitePattern))
                  else [count])
        if not counts:
            raise ValueError('loop count pattern cannot be empty')
        if len(counts) > 256:
            raise ValueError('loop count patterns are limited to 256 items')
        result = []
        for repetitions in counts:
            if isinstance(repetitions, bool) or not isinstance(repetitions, int):
                raise TypeError('loop counts must be whole numbers from 0 to 256')
            if not 0 <= repetitions <= 256:
                raise ValueError('loop counts must be whole numbers from 0 to 256')
            expanded = len(self) * repetitions
            if len(result) + expanded > 256:
                raise ValueError('loop output is limited to 256 items')
            for _ in range(repetitions):
                result.extend(self)
        return _FoxDotFinitePattern(result)

    def shuffle(self, count=1):
        if isinstance(count, bool) or not isinstance(count, int):
            raise TypeError('shuffle count must be a whole number from 0 to 256')
        if not 0 <= count <= 256:
            raise ValueError('shuffle count must be a whole number from 0 to 256')
        if len(self) * count > 256:
            raise ValueError('shuffle output is limited to 256 items')
        result = []
        for _ in range(count):
            result.extend(PShuf(self))
        return _FoxDotFinitePattern(result)

    def _shuffle_counts(self, count, name):
        counts = (list(count) if isinstance(
            count, (list, tuple, _FoxDotFinitePattern)) else [count])
        if not counts or len(counts) > 256:
            raise ValueError(name + ' requires 1 to 256 count values')
        for value in counts:
            if isinstance(value, bool) or not isinstance(value, int):
                raise TypeError(name + ' counts must be whole numbers from 0 to 256')
            if not 0 <= value <= 256:
                raise ValueError(name + ' counts must be whole numbers from 0 to 256')
        return counts

    def true_shuffle(self, count=1):
        counts = self._shuffle_counts(count, 'true_shuffle')

        expanded = []

        def expand(item, depth):
            if depth > 16:
                raise ValueError(
                    'true_shuffle is limited to 16 nested pattern levels')
            if isinstance(item, _FoxDotFinitePattern):
                for child in item:
                    expand(child, depth + 1)
            else:
                expanded.append(item)
                if len(expanded) > 256:
                    raise ValueError(
                        'true_shuffle input is limited to 256 expanded items')

        expand(self, 0)
        total = sum(counts)
        if len(expanded) * total > 256:
            raise ValueError('true_shuffle output is limited to 256 items')
        result = []
        source = _FoxDotFinitePattern(expanded)
        for repetitions in counts:
            for _ in range(repetitions):
                result.extend(PShuf(source))
        return _FoxDotFinitePattern(result)

    def shufflets(self, count=1):
        counts = self._shuffle_counts(count, 'shufflets')
        if len(self) > 8:
            raise ValueError('shufflets chord groups are limited to 8 notes')
        total = sum(counts)
        if total > 256:
            raise ValueError('shufflets output is limited to 256 groups')
        if total and not self:
            raise ValueError('shufflets requires a non-empty source pattern')
        if any(not _finite_note_number(value) for value in self):
            raise TypeError('shufflets supports numeric note patterns only')
        return _FoxDotFinitePattern([
            _FoxDotChord(*PShuf(self))
            for repetitions in counts for _ in range(repetitions)])

    def invert(self):
        return _invert_pitch_pattern(self)

    def arp(self, arp_pattern):
        sequences, kind = _compatible_pattern_sequences(
            (self, arp_pattern), 'arp', 2)
        if kind != 'numeric':
            raise TypeError('arp supports numeric note patterns and chords only')
        intervals = _FoxDotFinitePattern(sequences[1])
        return self.stutter(len(intervals)) + intervals

    def splice(self, seq, *seqs):
        sequences, _ = _compatible_pattern_sequences(
            (self, seq) + seqs, 'splice', 2)
        size = 1
        for sequence in sequences:
            size = size * len(sequence) // self._gcd(size, len(sequence))
            if size > 256 // len(sequences):
                raise ValueError('splice output is limited to 256 items')
        result = []
        for index in range(size):
            for sequence in sequences:
                result.append(sequence[index % len(sequence)])
        return _FoxDotFinitePattern(result)

    def sort(self):
        return _FoxDotFinitePattern(sorted(self))

    def unduplicate(self):
        result = []
        for item in self:
            if not result or item != result[-1]:
                result.append(item)
        return _FoxDotFinitePattern(result)

    def replace(self, old, new):
        def replace_item(item, depth):
            if depth > 16:
                raise ValueError('replace is limited to 16 nested pattern levels')
            if isinstance(item, _FoxDotFinitePattern):
                return _FoxDotFinitePattern([
                    replace_item(child, depth + 1) for child in item])
            if isinstance(item, _FoxDotChord):
                return _FoxDotChord(*[
                    replace_item(child, depth + 1) for child in item])
            if isinstance(item, tuple):
                return tuple(replace_item(child, depth + 1) for child in item)
            if isinstance(item, list):
                return [replace_item(child, depth + 1) for child in item]
            return new if item == old else item

        return _FoxDotFinitePattern([
            replace_item(item, 0) for item in self])

    def trim(self, size):
        if isinstance(size, bool) or not isinstance(size, int):
            raise TypeError('trim size must be a whole number')
        return _FoxDotFinitePattern(self[:size])

    def ltrim(self, size):
        if isinstance(size, bool) or not isinstance(size, int):
            raise TypeError('ltrim size must be a whole number')
        return _FoxDotFinitePattern(self[-size:])

    def normalise(self):
        if not self:
            raise ValueError('normalise requires a non-empty numeric pattern')
        if any(not _finite_note_number(value) for value in self):
            raise TypeError('normalise supports finite numeric patterns only')
        low = min(self)
        high = max(self)
        span = high - low
        if span == 0:
            return _FoxDotFinitePattern([0.0 for _ in self])
        result = []
        for value in self:
            normalized = (value - low) / span
            if not _finite_note_number(normalized):
                raise ValueError('normalise results must remain finite')
            result.append(normalized)
        return _FoxDotFinitePattern(result)

    def choose(self):
        if not self:
            raise ValueError('choose requires a non-empty finite pattern')
        return PRand(self, length=1)[0]

    def zip(self, other, dtype=None):
        if dtype is not None:
            raise TypeError('zip accepts only the built-in chord-group output')
        if isinstance(other, str):
            other = _FoxDotFinitePattern(list(other))
        elif not isinstance(other, (list, tuple, _FoxDotFinitePattern)):
            other = _FoxDotFinitePattern([other])
        return self._zip_with(other)

    def deepzip(self, other):
        return _deep_zip_pattern(self, other, reverse=False)

    def deeprzip(self, other):
        return _deep_zip_pattern(self, other, reverse=True)

    def map(self, mapping):
        if not isinstance(mapping, dict):
            raise TypeError('map requires a dictionary')
        if len(mapping) > 256:
            raise ValueError('map dictionaries are limited to 256 entries')
        budget = [0]

        def validate_value(value, depth):
            if depth > 16:
                raise ValueError('map is limited to 16 nested pattern levels')
            if isinstance(value, _FoxDotFinitePattern):
                if len(value) > 256:
                    raise ValueError('map values are limited to 256 items')
                return _FoxDotFinitePattern([
                    validate_value(child, depth + 1) for child in value])
            if isinstance(value, _FoxDotChord):
                values = [validate_value(child, depth + 1) for child in value]
                if all(_finite_note_number(child) for child in values):
                    return _FoxDotChord(*values)
                return tuple(values)
            if isinstance(value, tuple):
                if len(value) > 256:
                    raise ValueError('map values are limited to 256 items')
                return tuple(validate_value(child, depth + 1)
                             for child in value)
            if isinstance(value, list):
                if len(value) > 256:
                    raise ValueError('map values are limited to 256 items')
                return [validate_value(child, depth + 1) for child in value]
            budget[0] += 1
            if budget[0] > 256:
                raise ValueError('map output is limited to 256 values')
            if (value is None or isinstance(value, str) or
                    _finite_note_number(value)):
                return value
            raise TypeError(
                'map values must be finite numbers, rests, symbols, or groups')

        def map_item(item, depth):
            if depth > 16:
                raise ValueError('map is limited to 16 nested pattern levels')
            if isinstance(item, _FoxDotFinitePattern):
                return _FoxDotFinitePattern([
                    map_item(value, depth + 1) for value in item])
            if isinstance(item, _FoxDotChord):
                values = [map_item(value, depth + 1) for value in item]
                if all(_finite_note_number(value) for value in values):
                    return _FoxDotChord(*values)
                return tuple(values)
            if isinstance(item, tuple):
                return tuple(map_item(value, depth + 1) for value in item)
            if isinstance(item, list):
                return [map_item(value, depth + 1) for value in item]
            return validate_value(mapping.get(item, 0), depth)

        if len(self) > 256:
            raise ValueError('map input is limited to 256 top-level items')
        return _FoxDotFinitePattern([map_item(item, 0) for item in self])

    def pivot(self, index):
        indices = (list(index) if isinstance(
            index, (list, tuple, _FoxDotFinitePattern)) else [index])
        if len(indices) > 256:
            raise ValueError('pivot index patterns are limited to 256 items')
        if self and len(self) * len(indices) > 256:
            raise ValueError('pivot output is limited to 256 items')
        result = []
        length = len(self)
        for pivot_index in indices:
            if isinstance(pivot_index, bool) or not isinstance(pivot_index, int):
                raise TypeError('pivot indices must be whole numbers')
            if not length:
                continue
            midpoint = length / 2.0
            if pivot_index > midpoint:
                pivot_index = length - pivot_index - 1
                transformed = self.mirror().rotate(
                    (2 * (pivot_index % length)) + 1)
            else:
                transformed = self.rotate(
                    (2 * (pivot_index % length)) + 1).mirror()
            result.extend(transformed)
        return _FoxDotFinitePattern(result)

    def layer(self, method, *args, **kwargs):
        if not isinstance(method, str):
            raise ValueError(
                'layer requires an allowed pattern method name')
        methods = {
            'reverse': self.reverse,
            'rotate': self.rotate,
            'mirror': self.mirror,
            'invert': self.invert,
            'palindrome': self.palindrome,
            'stutter': self.stutter,
            'repeat': self.repeat,
            'stretch': self.stretch,
            'loop': self.loop,
            'shuffle': self.shuffle,
            'true_shuffle': self.true_shuffle,
            'shufflets': self.shufflets,
            'sort': self.sort,
            'unduplicate': self.unduplicate,
            'arp': self.arp,
            'splice': self.splice,
            'replace': self.replace,
            'trim': self.trim,
            'ltrim': self.ltrim,
            'normalise': self.normalise,
            'zip': self.zip,
            'deepzip': self.deepzip,
            'deeprzip': self.deeprzip,
            'map': self.map,
            'pivot': self.pivot,
        }
        if method not in methods:
            raise ValueError(
                'layer requires an allowed pattern method name')
        transformed = methods[method](*args, **kwargs)
        if not isinstance(transformed, _FoxDotFinitePattern):
            raise TypeError('layer transform must return a finite P pattern')
        return self._zip_with(transformed)

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

def _deep_zip_pattern(left, right, reverse=False):
    def is_sequence(value):
        return isinstance(value, (list, tuple, _FoxDotFinitePattern))

    budget = [0]

    def zip_level(left_value, right_value, depth):
        if depth > 16:
            raise ValueError('deep zip is limited to 16 nested pattern levels')
        left_items = (list(left_value) if is_sequence(left_value)
                      else [left_value])
        right_items = (list(right_value) if is_sequence(right_value)
                       else [right_value])
        if not left_items or not right_items:
            return _FoxDotFinitePattern()
        length = (len(left_items) * len(right_items) //
                  _FoxDotFinitePattern()._gcd(
                      len(left_items), len(right_items)))
        if length > 256 or budget[0] + length > 256:
            raise ValueError('deep zip output is limited to 256 paired items')
        budget[0] += length
        result = []
        for index in range(length):
            left_item = left_items[index % len(left_items)]
            right_item = right_items[index % len(right_items)]
            if is_sequence(left_item) or is_sequence(right_item):
                item = zip_level(left_item, right_item, depth + 1)
            else:
                item = ((right_item, left_item) if reverse else
                        (left_item, right_item))
            result.append(item)
        return _FoxDotFinitePattern(result)

    return zip_level(left, right, 0)

def _finite_note_number(value):
    return (isinstance(value, (int, float)) and not isinstance(value, bool) and
            value == value and value not in (float('inf'), float('-inf')))

def _invert_pitch_values(values, depth=0):
    if depth > 16:
        raise ValueError('invert is limited to 16 nested pattern levels')
    if len(values) > 256:
        raise ValueError('invert input patterns are limited to 256 items')
    notes = [value for value in values if _finite_note_number(value)]
    pivot = max(notes) if notes else 0
    result = []
    for value in values:
        if value is None:
            result.append(None)
        elif _finite_note_number(value):
            inverted = pivot - value
            if not _finite_note_number(inverted):
                raise ValueError('invert results must remain finite numbers')
            result.append(inverted)
        elif isinstance(value, (list, tuple, _FoxDotFinitePattern)):
            if not isinstance(value, _FoxDotFinitePattern) and not 1 <= len(value) <= 8:
                raise ValueError('invert chord groups must contain 1..8 notes')
            children = _invert_pitch_values(value, depth + 1)
            if isinstance(value, _FoxDotFinitePattern):
                result.append(_FoxDotFinitePattern(children))
            elif isinstance(value, tuple):
                result.append(_FoxDotChord(*children) if isinstance(value, _FoxDotChord)
                              else tuple(children))
            else:
                result.append(children)
        else:
            raise TypeError('invert supports finite numeric notes, rests, and numeric chords')
    return result

def _invert_pitch_pattern(pattern):
    return _FoxDotFinitePattern(_invert_pitch_values(pattern))

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

def _random_pattern_length(length, name):
    if isinstance(length, bool) or not isinstance(length, int):
        raise TypeError(name + ' length must be a whole number')
    if not 1 <= length <= 256:
        raise ValueError(name + ' length must be from 1 to 256')
    return length

def _random_pattern_seed(seed, name):
    global _dexter_random_counter
    if seed is None:
        _dexter_random_counter += 1
        seed = (int(_dexter_shuffle_seed) +
                _dexter_random_counter * 1013904223) & 0xffffffff
    elif isinstance(seed, bool) or not isinstance(seed, int):
        raise TypeError(name + ' seed must be a whole number')
    elif not 0 <= seed <= 0xffffffff:
        raise ValueError(name + ' seed must be from 0 to 4294967295')
    seed = int(seed) & 0xffffffff
    return seed if seed else 0x6d2b79f5

def _random_pattern_step(state):
    state ^= (state << 13) & 0xffffffff
    state &= 0xffffffff
    state ^= state >> 17
    state &= 0xffffffff
    state ^= (state << 5) & 0xffffffff
    state &= 0xffffffff
    return state, state / 4294967296.0

def PRand(start, stop=None, length=256, seed=None):
    length = _random_pattern_length(length, 'PRand')
    if isinstance(start, (str, list, tuple, _FoxDotFinitePattern)):
        if stop is not None:
            raise TypeError('PRand accepts one finite source pattern or integer bounds')
        values, kind = _pattern_items(start, 'PRand')
        if not values:
            raise ValueError('PRand source pattern cannot be empty')
        state = _random_pattern_seed(seed, 'PRand')
        result = []
        for _ in range(length):
            state, _ = _random_pattern_step(state)
            index = (state * len(values)) >> 32
            result.append(values[index])
        return _FoxDotFinitePattern(result)

    if isinstance(start, bool) or not isinstance(start, int):
        raise TypeError('PRand bounds must be whole numbers or a finite source pattern')
    low = 0 if stop is None else start
    high = start if stop is None else stop
    if isinstance(high, bool) or not isinstance(high, int):
        raise TypeError('PRand bounds must be whole numbers')
    if not -1000000 <= low <= high <= 1000000:
        raise ValueError('PRand bounds must be ordered and within -1000000..1000000')
    state = _random_pattern_seed(seed, 'PRand')
    span = high - low + 1
    result = []
    for _ in range(length):
        state, _ = _random_pattern_step(state)
        result.append(low + ((state * span) >> 32))
    return _FoxDotFinitePattern(result)

def PWhite(lo=0.0, hi=1.0, length=256, seed=None):
    length = _random_pattern_length(length, 'PWhite')
    for value in (lo, hi):
        if (isinstance(value, bool) or not isinstance(value, (int, float)) or
                value != value or value in (float('inf'), float('-inf')) or
                not -1000000.0 <= value <= 1000000.0):
            raise ValueError('PWhite bounds must be finite numbers within -1000000..1000000')
    lo, hi = float(lo), float(hi)
    if lo > hi:
        raise ValueError('PWhite requires lo <= hi')
    state = _random_pattern_seed(seed, 'PWhite')
    result = []
    for _ in range(length):
        state, first = _random_pattern_step(state)
        state, second = _random_pattern_step(state)
        # FoxDot PWhite uses a symmetric triangular distribution, not uniform.
        result.append(lo + ((first + second) * 0.5) * (hi - lo))
    return _FoxDotFinitePattern(result)

def PWalk(max=7, step=1, start=0, length=256, seed=None):
    length = _random_pattern_length(length, 'PWalk')
    for label, value in (('max', max), ('start', start)):
        if (isinstance(value, bool) or not isinstance(value, (int, float)) or
                value != value or value in (float('inf'), float('-inf')) or
                not -1000000.0 <= value <= 1000000.0):
            raise ValueError('PWalk ' + label + ' must be finite and within -1000000..1000000')
    if isinstance(step, (list, tuple, _FoxDotFinitePattern)):
        stepValues, stepKind = _pattern_items(step, 'PWalk step')
        if stepKind != 'numeric' or not stepValues or not all(
                _finite_note_number(value) for value in stepValues):
            raise TypeError('PWalk step requires a non-empty numeric pattern')
    elif _finite_note_number(step):
        stepValues = [step]
    else:
        raise TypeError('PWalk step must be a finite number or numeric pattern')
    stepValues = [abs(float(value)) for value in stepValues]
    if any(value > 1000000.0 for value in stepValues):
        raise ValueError('PWalk step values must be within -1000000..1000000')

    boundary = abs(float(max))
    value = float(start)
    state = _random_pattern_seed(seed, 'PWalk')
    result = []
    for index in range(length):
        if index:
            if value >= boundary:
                direction = -1.0
            elif value <= -boundary:
                direction = 1.0
            else:
                state, choice = _random_pattern_step(state)
                direction = 1.0 if choice < 0.5 else -1.0
            value += direction * stepValues[index % len(stepValues)]
            if not _finite_note_number(value):
                raise ValueError('PWalk generated a non-finite note')
        result.append(value)
    return _FoxDotFinitePattern(result)

def PChain(mapping, length=256, seed=None):
    length = _random_pattern_length(length, 'PChain')
    if not isinstance(mapping, dict):
        raise TypeError('PChain expects a dictionary of transition choices')
    if not 1 <= len(mapping) <= 32:
        raise ValueError('PChain mappings must contain 1 to 32 states')

    normalized = {}
    kind = None
    choiceCount = 0
    for key, rawChoices in mapping.items():
        if isinstance(key, str) and len(key) == 1 and (
                key == ' ' or key in _SAMPLE_SYMBOLS):
            keyKind = 'sample'
        elif _finite_note_number(key) and abs(float(key)) <= 1000000.0:
            keyKind = 'numeric'
        else:
            raise TypeError(
                'PChain keys must be finite numeric notes or single sample symbols')
        if kind is not None and keyKind != kind:
            raise TypeError('PChain cannot mix numeric notes and sample symbols')
        kind = keyKind

        if isinstance(rawChoices, _FoxDotFinitePattern):
            choices = list(rawChoices)
        elif isinstance(rawChoices, list):
            choices = list(rawChoices)
        elif isinstance(rawChoices, tuple):
            choices = [rawChoices]
        else:
            choices = [rawChoices]
        if not choices:
            raise ValueError('PChain transition choices cannot be empty')
        choiceCount += len(choices)
        if choiceCount > 256:
            raise ValueError('PChain transition choices are limited to 256 items')
        choiceValues, choiceKind = _pattern_items(choices, 'PChain')
        if choiceKind != keyKind:
            raise TypeError('PChain transitions must match their key type')
        normalized[key] = choiceValues

    state = _random_pattern_seed(seed, 'PChain')
    value = list(normalized.keys())[0]
    result = []
    for index in range(length):
        if index:
            choices = normalized.get(value)
            if choices:
                state, _ = _random_pattern_step(state)
                choiceIndex = (state * len(choices)) >> 32
                value = choices[choiceIndex]
        result.append(value)
    return _FoxDotFinitePattern(result)

def PDelta(deltas, start=0, length=256):
    length = _random_pattern_length(length, 'PDelta')
    if not _finite_note_number(start) or abs(float(start)) > 1000000.0:
        raise ValueError('PDelta start must be finite and within -1000000..1000000')
    if _finite_note_number(deltas):
        deltaValues = [deltas]
    else:
        deltaValues, deltaKind = _pattern_items(deltas, 'PDelta')
        if not deltaValues:
            raise ValueError('PDelta delta pattern cannot be empty')
        if deltaKind != 'numeric':
            raise TypeError('PDelta requires a numeric delta pattern')
    if not deltaValues:
        raise ValueError('PDelta delta pattern cannot be empty')
    if any(not _finite_note_number(delta) or abs(float(delta)) > 1000000.0
           for delta in deltaValues):
        raise ValueError('PDelta deltas must be finite and within -1000000..1000000')

    value = start
    result = []
    for index in range(length):
        if index:
            value += deltaValues[(index - 1) % len(deltaValues)]
        if not _finite_note_number(value) or abs(float(value)) > 1000000.0:
            raise ValueError('PDelta accumulated notes must stay within -1000000..1000000')
        result.append(value)
    return _FoxDotFinitePattern(result)

def PSquare(length=256):
    length = _random_pattern_length(length, 'PSquare')
    return _FoxDotFinitePattern(index * index for index in range(length))

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
         _allow_repeated_notes=False, _degree_control=None, _root_control=None):
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
        if _degree_control is not None:
            degree = (int(round(_degree_control.at(start))) +
                      (_base_octave - 5) * _degrees_per_octave)
            if len(chord) > 1:
                anchor = int(chord[0])
                chord = [degree + (int(note) - anchor) for note in chord]
            else:
                chord = [degree]
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
                rootControl = (_root_control if _root_control is not None
                               else _dexter_root_control)
                rootMidi = (int(round(rootControl.at(start)))
                            if rootControl is not None else _dexter_root)
                note = rootMidi + octave * 12 + _dexter_scale[degree]
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

def _normalize_root_control(value):
    if not isinstance(value, _DexterTimeVar):
        return (None, None) if value is None else (_root_midi(value), None)
    values = value.values
    if any(not float(item).is_integer() for item in values):
        raise ValueError('root var/linvar endpoints must be whole pitch classes or MIDI notes')
    if all(0.0 <= item <= 11.0 for item in values):
        midiValues = [60 + int(item) for item in values]
    elif all(12.0 <= item <= 127.0 for item in values):
        midiValues = [int(item) for item in values]
    else:
        raise ValueError(
            'root var/linvar must use pitch classes 0..11 or MIDI notes 12..127')
    control = _DexterTimeVar(midiValues, value.durations, value.linear)
    return midiValues[0], control

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
        self.rootMidi, self.rootControl = _normalize_root_control(root)
        if synth not in _DEXTER_BUILTIN_SYNTHS and \
                synth not in _NEW_SYNTHS and \
                synth not in _dexter_user_synth_names:
            raise ValueError('Unknown embedded synth: ' + str(synth))
        self.synth = synth
        self.playerEffectUserIndex = -1
        self.synthControls = (0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0)
        self.effectControlVariables = ('-', '-', '-', '-', '-', '-', '-')
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
        self.degreeControl = None
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

def userSynth(name, notes=0, **kwargs):
    if not isinstance(name, str) or name not in _dexter_user_synth_names:
        raise ValueError('userSynth name is not a loaded Portable/SynthDefs entry')
    controls = []
    specifications = (
        ('vib', 0.0, 0.0, 100.0),
        ('depth', 0.5, 0.0, 10.0), ('lforate', 1.0, 0.0, 100.0),
        ('lfowidth', 0.5, 0.0, 1.0), ('rq', 0.5, 0.01, 3.0),
        ('fmod', 0.0, -2000.0, 2000.0), ('blur', 1.0, 0.01, 8.0))
    for key, default, minimum, maximum in specifications:
        value = float(kwargs.pop(key, default))
        if value != value or not minimum <= value <= maximum:
            raise ValueError(key + ' must be a finite scalar in the supported range')
        controls.append(value)
    pattern = _fox_wave(name, notes, **kwargs)
    pattern.synthControls = tuple(controls)
    if name in ('AirwindowsTapeDelay2', 'AirwindowsEchojam'):
        pattern.playerEffectUserIndex = _dexter_user_synth_names.index(name)
    return pattern

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

def _sample_fraction_pattern(value, name):
    if isinstance(value, _DexterTimeVar):
        values = value.values
        pattern = value
        patternLength = None
    elif isinstance(value, bool):
        raise ValueError('sample ' + name + ' must contain numeric fractions')
    elif isinstance(value, (int, float)):
        values = [value]
        pattern = float(value)
        patternLength = None
    elif isinstance(value, (list, tuple)):
        if not value:
            raise ValueError('sample ' + name + ' pattern cannot be empty')
        if len(value) > 256:
            raise ValueError('sample ' + name + ' pattern is limited to 256 values')
        values = list(value)
        pattern = []
        for item in values:
            if isinstance(item, bool) or not isinstance(item, (int, float)):
                raise ValueError('sample ' + name + ' must contain numeric fractions')
            pattern.append(float(item))
        values = pattern
        patternLength = len(pattern)
    else:
        raise ValueError('sample ' + name + ' must be a number, P/list pattern, or var/linvar')

    for item in values:
        fraction = float(item)
        if (fraction != fraction or fraction in (float('inf'), float('-inf')) or
                not 0.0 <= fraction <= 1.0):
            raise ValueError('sample ' + name + ' fractions must be finite values from 0 to 1')
    return pattern, patternLength

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

def _sample_effect_control_values(effect, controls):
    keys = ('fx_vib', 'fx_depth', 'fx_lforate',
            'fx_lfowidth', 'fx_rq', 'fx_blur')
    provided = [key for key in keys if key in controls]
    if effect is None:
        if provided:
            raise ValueError('fx_* controls require effect="AirwindowsTapeDelay2" or effect="AirwindowsEchojam"')
        return None, ('-', '-', '-', '-', '-', '-', '-')

    if effect == 'AirwindowsTapeDelay2':
        specs = (
            ('fx_vib', 0, 0.94), ('fx_depth', 1, 0.28),
            ('fx_lforate', 2, 0.72), ('fx_lfowidth', 3, 0.28),
            ('fx_rq', 4, 0.12), ('fx_blur', 6, 0.5))
    elif effect == 'AirwindowsEchojam':
        specs = (
            ('fx_vib', 0, 0.29), ('fx_depth', 1, 0.32),
            ('fx_lforate', 2, 0.48), ('fx_blur', 6, 0.62))
        unsupported = [key for key in provided
                       if key in ('fx_lfowidth', 'fx_rq')]
        if unsupported:
            raise ValueError(unsupported[0] + ' is not a control on AirwindowsEchojam')
    else:
        raise ValueError('unsupported Airwindows player effect')

    values = [0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0]
    variables = ['-', '-', '-', '-', '-', '-', '-']
    for key, index, default in specs:
        raw = controls.pop(key, default)
        if isinstance(raw, _DexterTimeVar):
            if any(value < 0.0 or value > 1.0 for value in raw.values):
                raise ValueError(key + ' time-var values must be between 0 and 1')
            values[index] = raw.values[0]
            variables[index] = raw.wire()
            continue
        if isinstance(raw, bool):
            raise ValueError(key + ' must be a finite number or var/linvar from 0 to 1')
        try:
            value = float(raw)
        except (TypeError, ValueError, OverflowError):
            raise ValueError(key + ' must be a finite number or var/linvar from 0 to 1')
        if not 0.0 <= value <= 1.0:
            raise ValueError(key + ' must be a finite number or var/linvar from 0 to 1')
        values[index] = value
    return tuple(values), tuple(variables)

def _sample_pattern(text, dur=1.0, sus=None, sample=0, rate=1.0,
                    start=0.0, end=1.0, loop=False, effect=None, **controls):
    playerEffectUserIndex = -1
    if effect is not None:
        if effect not in ('AirwindowsTapeDelay2', 'AirwindowsEchojam'):
            raise ValueError('sample effect must be AirwindowsTapeDelay2 or AirwindowsEchojam')
        if effect not in _dexter_user_synth_names:
            raise ValueError(
                'sample effect ' + effect +
                ' is not loaded; install its SynthDef in Portable/SynthDefs and restart the DAW')
        playerEffectUserIndex = _dexter_user_synth_names.index(effect)
    effectControlValues, effectControlVariables = _sample_effect_control_values(effect, controls)
    start, startPatternLength = _sample_fraction_pattern(start, 'start')
    end, endPatternLength = _sample_fraction_pattern(end, 'end')
    if not isinstance(loop, bool):
        raise ValueError('sample loop must be True or False')
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
    if startPatternLength is not None:
        lengths.append(startPatternLength)
    if endPatternLength is not None:
        lengths.append(endPatternLength)
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
    pattern.sampleSpec = (selections, rates, sus is not None or loop, positions,
                          sampleControl, rateControl, symbolEvents,
                          start, end, loop)
    pattern.playerEffectUserIndex = playerEffectUserIndex
    if effectControlValues is not None:
        pattern.synthControls = effectControlValues
        pattern.effectControlVariables = effectControlVariables
    return pattern

def play(notes, *args, **kwargs):
    if isinstance(notes, (str, _FoxDotFinitePattern)):
        return _sample_pattern(notes, *args, **kwargs)
    return _play_notes(notes, *args, **kwargs)

_GROUP_PATTERN_FIELDS = {
    'amp': 'amp', 'pan': 'pan', 'cutoff': 'cutoff',
    'echo': 'echoBeats', 'delaytime': 'delayTimeSeconds',
    'atk': 'attackSeconds', 'rel': 'releaseSeconds',
    'reverb': 'reverb', 'room': 'room', 'mix': 'mix', 'damp': 'damp',
    'revtime': 'revtime', 'spread': 'spread', 'lpf': 'lpfHz',
    'lpr': 'lpr', 'hpf': 'hpfHz', 'hpr': 'hpr', 'bpf': 'bpfHz',
    'bpr': 'bpr', 'decay': 'echoDecayBeats', 'echomix': 'echoMix',
    'feedback': 'feedback', 'bpnoise': 'bpnoise', 'swell': 'swell',
    'degree': 'degreeControl', 'dur': 'step', 'sus': 'duration',
    'oct': 'octaveControl', 'scale': 'scaleName', 'root': 'rootMidi'
}

def _group_degree_items(value):
    if isinstance(value, _DexterTimeVar):
        if any(item < -128.0 or item > 127.0 for item in value.values):
            raise ValueError('Group.degree var/linvar values must be from -128 to 127')
        return value
    if isinstance(value, _FoxDotChord):
        items = [value]
    elif value is None or _finite_note_number(value):
        items = [value]
    elif isinstance(value, (list, tuple, _FoxDotFinitePattern)):
        items = list(value)
    else:
        raise TypeError('Group.degree requires finite numeric notes, rests, or chords')
    if not items or len(items) > 256:
        raise ValueError('Group.degree is limited to 1..256 note steps')
    _, kind = _pattern_items(items, 'Group.degree')
    if kind not in (None, 'numeric'):
        raise TypeError('Group.degree accepts numeric note patterns, not sample symbols')
    return _FoxDotFinitePattern(items)

def _group_shift_notes(notes, semitones):
    def shift_item(item):
        if item is None:
            return None
        if isinstance(item, (list, tuple)):
            values = [shift_item(value) for value in item]
            return tuple(values) if isinstance(item, tuple) else values
        if not _finite_note_number(item):
            raise TypeError('Group pitch controls require finite numeric degrees')
        return item + semitones
    return [shift_item(item) for item in notes]

def _validate_group_time_pattern(value, name):
    lower = 0.001 if name == 'dur' else 0.02
    source = value.values if isinstance(value, _DexterTimeVar) else value
    if isinstance(source, (int, float)) and not isinstance(source, bool):
        items = [source]
    elif isinstance(source, _FoxDotDurationGroup):
        items = [source]
    elif name == 'sus' and isinstance(source, _FoxDotChord):
        items = [source]
    elif isinstance(source, (list, tuple, _FoxDotFinitePattern)):
        items = list(source)
    else:
        raise TypeError('Group.' + name + ' requires a number or bounded pattern')
    if not items or len(items) > 256:
        raise ValueError('Group.' + name + ' is limited to 1..256 values')
    for item in items:
        values = list(item) if name == 'sus' and isinstance(
            item, _FoxDotChord) else [item]
        if isinstance(item, _FoxDotDurationGroup):
            if name != 'dur' or not item:
                raise TypeError('PRhythm duration groups are supported only in Group.dur')
            base = float(item[0])
            offsets = [float(offset) for offset in item[1:]]
            if (base != base or not lower <= base <= 4096.0 or
                    len(offsets) > 255 or
                    any(offset != offset or offset < 0.0 or
                        offset > 8.0 or offset > base for offset in offsets)):
                raise ValueError('Group.dur PRhythm values are outside the supported bounds')
            continue
        for numeric in values:
            if (isinstance(numeric, bool) or
                    not isinstance(numeric, (int, float))):
                raise TypeError('Group.' + name + ' values must be numeric')
            numeric = float(numeric)
            if (numeric != numeric or numeric in
                    (float('inf'), float('-inf')) or
                    not lower <= numeric <= 4096.0):
                raise ValueError(
                    'Group.' + name + ' values must be between ' +
                    str(lower) + ' and 4096 beats')

def _normalize_group_control(name, value):
    if name == 'root':
        return _normalize_root_control(value)
    if name == 'degree':
        return _group_degree_items(value)
    if name in ('dur', 'sus'):
        _validate_group_time_pattern(value, name)
        return value
    if name == 'oct':
        if isinstance(value, _DexterTimeVar):
            if any(item < 0.0 or item > 10.0 for item in value.values):
                raise ValueError('Group.oct values must be from 0 to 10')
            return value
        if isinstance(value, bool) or not _finite_note_number(value):
            raise TypeError('Group.oct requires a finite number or var/linvar')
        octave = int(value)
        if not 0 <= octave <= 10:
            raise ValueError('Group.oct must be from 0 to 10')
        return octave
    if name == 'scale':
        if isinstance(value, _DexterTimeVar):
            raise TypeError('Group.scale does not accept var/linvar')
        return _scale_intervals(value)
    return value

def _set_group_pattern_control(pattern, name, value):
    if name == 'root':
        if pattern.synth == 'sample' or not pattern.degrees:
            raise ValueError('Group.root applies only to scale-degree note players')
        pattern.rootMidi, pattern.rootControl = value
    elif name == 'degree':
        if pattern.synth == 'sample':
            raise ValueError('Group.degree applies to numeric note players, not sample players')
        if not pattern.degrees:
            raise ValueError('Group.degree applies only to scale-degree note players')
        if isinstance(value, _DexterTimeVar):
            pattern.degreeControl = value
            return
        pattern.degreeControl = None
        shift = (pattern.baseOctave - 5) * pattern.degreesPerOctave
        pattern.notes = _group_shift_notes(value, shift)
    elif name == 'dur':
        pattern.step = value
    elif name == 'sus':
        pattern.duration = value
    elif name == 'oct':
        if pattern.synth == 'sample' or not pattern.degrees:
            raise ValueError('Group.oct applies only to scale-degree note players')
        if isinstance(value, _DexterTimeVar):
            baseOctave = int(round(value.values[0]))
            octaveControl = value
        else:
            baseOctave = int(value)
            octaveControl = None
        oldShift = (pattern.baseOctave - 5) * pattern.degreesPerOctave
        rawNotes = _group_shift_notes(pattern.notes, -oldShift)
        newShift = (baseOctave - 5) * pattern.degreesPerOctave
        pattern.notes = _group_shift_notes(rawNotes, newShift)
        pattern.baseOctave = baseOctave
        pattern.octaveControl = octaveControl
    elif name == 'scale':
        if pattern.synth == 'sample' or not pattern.degrees:
            raise ValueError('Group.scale applies only to scale-degree note players')
        oldShift = (pattern.baseOctave - 5) * pattern.degreesPerOctave
        rawNotes = _group_shift_notes(pattern.notes, -oldShift)
        intervals = _scale_intervals(value)
        pattern.scaleName = intervals
        pattern.degreesPerOctave = len(intervals)
        newShift = (pattern.baseOctave - 5) * pattern.degreesPerOctave
        pattern.notes = _group_shift_notes(rawNotes, newShift)
    elif name == 'amp':
        pattern.amp, pattern.ampControl = _control_value(value, 'amp', 0.0, 1.0)
    elif name == 'pan':
        pattern.pan, pattern.panControl = _control_value(value, 'pan', -1.0, 1.0)
        pattern.mousePan = False
    elif name == 'cutoff':
        pattern.cutoff, pattern.cutoffControl = _control_value(
            value, 'cutoff', 20.0, 20000.0)
        pattern.mouseCutoff = False
    elif name == 'echo':
        pattern.echoBeats, pattern.echoControl = _control_value(
            value, 'echo', 0.0, 64.0)
    elif name == 'delaytime':
        pattern.delayTimeSeconds, pattern.delayTimeControl = _control_value(
            value, 'delaytime', 0.0, 4.0)
    elif name == 'atk':
        pattern.attackSeconds, pattern.attackControl = _event_control(
            value, 'atk', 0.0, 3.0)
    elif name == 'rel':
        pattern.releaseSeconds, pattern.releaseControl = _event_control(
            value, 'rel', 0.0, 3.0)
    elif name == 'reverb':
        if value not in ('freeverb', 'freeverb2', 'gverb', 'none'):
            raise ValueError('reverb must be freeverb, freeverb2, gverb, or none')
        pattern.reverb = value
        pattern.reverbIndex = ('none', 'freeverb', 'freeverb2', 'gverb').index(value)
    elif name in ('room', 'mix', 'damp'):
        result = float(value)
        if result != result or not 0.0 <= result <= 1.0:
            raise ValueError(name + ' must be between 0 and 1')
        if name == 'room':
            pattern.room = result
        elif name == 'mix':
            pattern.mix = result
        else:
            pattern.damp = result
    elif name == 'revtime':
        result = float(value)
        if result != result or not 0.1 <= result <= 10.0:
            raise ValueError('revtime must be between 0.1 and 10 seconds')
        pattern.revtime = result
    elif name == 'spread':
        result = float(value)
        if result != result or not 0.0 <= result <= 100.0:
            raise ValueError('spread must be between 0 and 100')
        pattern.spread = result
    elif name in ('lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr'):
        lower, upper = ((0.05, 1.0) if name in ('lpr', 'hpr', 'bpr')
                        else (0.0, 20000.0))
        normalized, control = _event_control(value, name, lower, upper)
        if name == 'lpf':
            pattern.lpfHz, pattern.lpfControl = normalized, control
        elif name == 'lpr':
            pattern.lpr, pattern.lprControl = normalized, control
        elif name == 'hpf':
            pattern.hpfHz, pattern.hpfControl = normalized, control
        elif name == 'hpr':
            pattern.hpr, pattern.hprControl = normalized, control
        elif name == 'bpf':
            pattern.bpfHz, pattern.bpfControl = normalized, control
        else:
            pattern.bpr, pattern.bprControl = normalized, control
    elif name == 'decay':
        pattern.echoDecayBeats, pattern.echoDecayControl = _event_control(
            value, 'decay', 0.0, 64.0)
    elif name == 'echomix':
        pattern.echoMix, pattern.echoMixControl = _control_value(
            value, 'echomix', 0.0, 1.0)
    elif name == 'feedback':
        if value is None:
            pattern.feedback, pattern.feedbackControl = -1.0, None
        else:
            pattern.feedback, pattern.feedbackControl = _control_value(
                value, 'feedback', 0.0, 0.95)
    elif name == 'bpnoise':
        pattern.bpnoise, pattern.bpnoiseControl = _event_control(
            value, 'bpnoise', 0.0, 10.0)
    elif name == 'swell':
        pattern.swell, pattern.swellControl = _event_control(
            value, 'swell', 0.0, 4.0)
    else:
        raise AttributeError('unsupported shared player control ' + name)

class Player:
    def __init__(self, number):
        self.number = number
        self._currentPattern = None
        self._active = False
        self._groupOverrides = {}
        self._follows = None
        self._followers = []
        self._applyingFollow = False
        self._baseUpdate = None
        self._scheduledTransforms = []

    def __getattr__(self, name):
        if name == 'degree':
            if self._currentPattern is None:
                raise ValueError('Assign a pattern to this player before referencing its degree.')
            return _PlayerDegreeReference(self)
        raise AttributeError(name)

    def __setattr__(self, name, value):
        if name == 'degree':
            if not isinstance(value, _PlayerDegreeReference):
                raise TypeError('Player degree references must use another player, e.g. p2.degree = p1.degree.')
            self.follow(value.player)
            return
        self.__dict__[name] = value

    def __rshift__(self, pattern):
        global _dexter_events, _dexter_length, _dexter_slots
        global _dexter_default_touched, _dexter_scale, _dexter_root
        global _dexter_root_control
        global _dexter_sample_controls
        if not isinstance(pattern, PlayerPattern):
            raise TypeError('assign sequence(...) to a player using >>')
        if self._groupOverrides:
            copiedPattern = PlayerPattern.__new__(PlayerPattern)
            copiedPattern.__dict__ = dict(pattern.__dict__)
            for controlName, controlValue in self._groupOverrides.items():
                _set_group_pattern_control(
                    copiedPattern, controlName, controlValue)
            pattern = copiedPattern
        saved = (_dexter_events, _dexter_length, _dexter_slots,
                 _dexter_default_touched, _dexter_scale, _dexter_root,
                 _dexter_root_control)
        _dexter_events = []
        _dexter_length = 1.0
        _dexter_slots = 0
        _dexter_default_touched = False
        try:
            if pattern.scaleName is not None:
                _dexter_scale = _scale_intervals(pattern.scaleName)
            if pattern.rootMidi is not None:
                _dexter_root = pattern.rootMidi
                _dexter_root_control = pattern.rootControl
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
                   _degree_control=pattern.degreeControl,
                    _root_control=pattern.rootControl,
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
            _dexter_root_control = saved[6]
        playerIndex = self.number - 1
        if not 0 <= playerIndex < 8:
            raise ValueError('player number must be 1..8')
        synthIndex = (_DEXTER_BUILTIN_SYNTHS.index(pattern.synth)
                      if pattern.synth in _DEXTER_BUILTIN_SYNTHS else
                      len(_DEXTER_BUILTIN_SYNTHS) +
                      _dexter_user_synth_names.index(pattern.synth))
        if pattern.synth == 'sample':
            requests = []
            adjusted = []
            assignedMidiIntervals = []
            (selection, rates, gate, positions, sampleControl, rateControl,
             symbolEvents, sampleStart, sampleEnd, sampleLoop) = pattern.sampleSpec
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
                sliceStart = float(_sample_value(sampleStart, position, event[0], 'start'))
                sliceEnd = float(_sample_value(sampleEnd, position, event[0], 'end'))
                if not 0.0 <= sliceStart < sliceEnd <= 1.0:
                    raise ValueError(
                        'sample start/end must satisfy 0 <= start < end <= 1 at every hit')
                requests.append((symbol, int(sampleIndex), playbackRate, int(gate),
                                 sliceStart, sliceEnd, int(sampleLoop)))
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
            _dexter_sample_base_events[playerIndex] = list(requests)
            _dexter_sample_controls[playerIndex] = (
                sampleControl.wire() if sampleControl is not None else '-',
                rateControl.wire() if rateControl is not None else '-')
        _dexter_synth_controls[playerIndex] = pattern.synthControls
        _dexter_effect_control_variables[playerIndex] = pattern.effectControlVariables
        effectUserIndex = pattern.playerEffectUserIndex
        _dexter_player_effect_indices[playerIndex] = effectUserIndex
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
        self._currentPattern = pattern
        self._baseUpdate = _dexter_updates[playerIndex]
        if self._scheduledTransforms:
            self._apply_scheduled_transform()
        if self._follows is not None and not self._applyingFollow:
            self._copy_followed_degree(self._follows)
        self._refresh_followers()
        self._active = True
        return self

    def _apply_sample_stutter(self, operations):
        base = self._baseUpdate
        pattern = self._currentPattern
        playerIndex = self.number - 1
        if any(operation[0] != 'stutter' for operation in operations):
            raise ValueError('Sample players currently support scheduled stutter only.')
        events = list(base[3])
        requests = list(_dexter_sample_base_events.get(playerIndex, ()))
        if not events or len(events) != len(requests):
            raise ValueError('Sample stutter needs matching playable hits and sample requests.')
        if (pattern.strumControl is not None or pattern.strumBeats != 0.0 or
                pattern.delayControl is not None or pattern.delayBeats != 0.0):
            raise ValueError('Sample stutter currently needs unstrummed hits without per-note delay.')
        baseLength = float(base[2])
        if not 0.0 < baseLength <= 4096.0:
            raise ValueError('Sample stutter phrase length must be between 0 and 4096 beats.')
        def hitsAt(beat):
            phase = float(beat) % baseLength
            hits = []
            for event, request in zip(events, requests):
                start = (float(event[0]) + float(event[6])) % baseLength
                distance = ((start - phase + baseLength * 0.5) %
                            baseLength) - baseLength * 0.5
                if abs(distance) <= 1e-8:
                    hits.append((event, request))
            return hits

        schedules = [
            self._compile_scheduled_operation(
                operation, baseLength, len(events))
            for operation in operations
        ]
        longestPeriod = max(schedule['period'] for schedule in schedules)
        commonLength = None
        maxMultiplier = max(1, int(4096.0 / longestPeriod))
        for multiplier in range(1, maxMultiplier + 1):
            candidate = longestPeriod * multiplier
            if all(abs(candidate / schedule['period'] -
                       round(candidate / schedule['period'])) <= 1e-8
                   for schedule in schedules):
                commonLength = candidate
                break
        if commonLength is None:
            raise ValueError('Combined sample stutters do not form a repeating phrase within 4096 beats.')
        cycles = round(commonLength / baseLength)
        if (cycles < 1 or
                abs(commonLength - cycles * baseLength) > 1e-7):
            raise ValueError('Combined sample stutters do not align with the drum pattern.')

        scheduledBeats = []
        for scheduleIndex, schedule in enumerate(schedules):
            localCycles = round(commonLength / schedule['period'])
            callLimit = (len(schedule['calls']) - 1
                         if schedule['closureAtPeriod'] and
                         schedule['method'] != 'stutter'
                         else len(schedule['calls']))
            for localCycle in range(localCycles):
                cycleOffset = localCycle * schedule['period']
                for callIndex in range(callLimit):
                    beat = cycleOffset + schedule['calls'][callIndex]
                    if beat >= commonLength - 1e-8:
                        if (schedule['closureAtPeriod'] and
                                schedule['method'] == 'stutter' and
                                abs(beat - commonLength) <= 1e-8):
                            beat = 0.0
                        else:
                            continue
                    scheduledBeats.append((beat, scheduleIndex))
                    if len(scheduledBeats) > 256:
                        raise ValueError('Combined sample stutters exceed 256 scheduled calls.')
        scheduledBeats.sort(key=lambda item: (item[0], item[1]))
        stutterCalls = [
            (beat, scheduleIndex, hitsAt(beat))
            for beat, scheduleIndex in scheduledBeats
        ]
        extraCount = sum(
            len(hits) * (schedules[scheduleIndex]['amount'] - 1)
            for _, scheduleIndex, hits in stutterCalls)
        if len(events) * cycles + extraCount > 256:
            raise ValueError('Sample stutter expands beyond the 256-event limit; use fewer hits, fewer triggers, or a smaller stutter count.')

        outputPairs = []
        for cycleIndex in range(cycles):
            cycleOffset = cycleIndex * baseLength
            for event, request in zip(events, requests):
                repeatedEvent = list(event)
                repeatedEvent[0] = float(event[0]) + cycleOffset
                outputPairs.append((tuple(repeatedEvent), request))

        hitStarts = sorted(set(
            (float(event[0]) + float(event[6])) % baseLength
            for event in events))
        for scheduledBeat, scheduleIndex, hits in stutterCalls:
            if not hits:
                continue
            amount = schedules[scheduleIndex]['amount']
            callBeat = scheduledBeat % commonLength
            phase = callBeat % baseLength
            nextStarts = [start for start in hitStarts
                          if start > phase + 1e-8]
            nextHit = (nextStarts[0] if nextStarts else
                       hitStarts[0] + baseLength)
            remaining = nextHit - phase
            if remaining <= 1e-9:
                continue
            spacing = remaining / float(amount)
            for event, request in hits:
                for repeatIndex in range(1, amount):
                    repeatedEvent = list(event)
                    repeatedStart = (
                        callBeat + spacing * repeatIndex) % commonLength
                    repeatedEvent[0] = repeatedStart - float(event[6])
                    repeatedEvent[1] = min(float(event[1]), spacing)
                    outputPairs.append((tuple(repeatedEvent), request))

        outputPairs.sort(
            key=lambda item: float(item[0][0]) + float(item[0][6]))
        expanded = list(base)
        expanded[2] = commonLength
        expanded[3] = [event for event, _ in outputPairs]
        _dexter_updates[playerIndex] = tuple(expanded)
        _dexter_sample_events[playerIndex] = [
            request for _, request in outputPairs]

    def _compile_scheduled_operation(self, operation, baseLength, eventCount):
        (method, amount, intervals, intervalRange, phase, repeatCycle,
         scheduleKey, explicitCycle) = operation
        calls = []
        elapsed = 0.0
        cycles = 0
        reverseState = False
        rotation = 0
        shuffleOrders = []
        if explicitCycle:
            for cycleIndex in range(1, 257):
                callBeat = (
                    float(phase[(cycleIndex - 1) % len(phase)]) +
                    (cycleIndex - 1) * float(repeatCycle))
                calls.append(callBeat)
                if method == 'reverse':
                    reverseState = not reverseState
                elif method == 'rotate':
                    rotation = (rotation + amount) % eventCount
                elif method == 'shuffle':
                    shuffleOrders.append(list(
                        PShuf(_FoxDotFinitePattern(list(range(eventCount))))))
                elapsed = cycleIndex * float(repeatCycle)
                if elapsed > 4096.0:
                    break
                candidateCycles = round(elapsed / baseLength)
                aligned = abs(elapsed - candidateCycles * baseLength) <= 1e-8
                reset = (
                    not reverseState if method == 'reverse' else
                    rotation == 0 if method == 'rotate' else
                    True)
                if (cycleIndex % len(phase) != 0 or
                        candidateCycles < 1 or not aligned or not reset):
                    continue
                if (method == 'stutter' and
                        eventCount * candidateCycles +
                        len(calls) * (amount - 1) > 256):
                    continue
                elapsed = candidateCycles * baseLength
                cycles = candidateCycles
                return {
                    'method': method,
                    'amount': amount,
                    'calls': tuple(calls),
                    'period': elapsed,
                    'shuffleOrders': tuple(shuffleOrders),
                    'closureAtPeriod': False,
                }
            raise ValueError('Scheduled operation could not form a bounded repeating phrase; use a shorter cycle or another interval.')
        for index in range(256):
            elapsed += float(intervals[index % len(intervals)])
            calls.append(elapsed)
            if elapsed > 4096.0:
                break
            if method == 'reverse':
                reverseState = not reverseState
            elif method == 'rotate':
                rotation = (rotation + amount) % eventCount
            elif method == 'shuffle':
                shuffleOrders.append(list(
                    PShuf(_FoxDotFinitePattern(list(range(eventCount))))))
            candidateCycles = round(elapsed / baseLength)
            aligned = abs(elapsed - candidateCycles * baseLength) <= 1e-8
            reset = (
                not reverseState if method == 'reverse' else
                rotation == 0 if method == 'rotate' else
                True)
            if candidateCycles < 1 or not reset:
                continue
            if not aligned:
                previousElapsed = (
                    elapsed - float(intervals[index % len(intervals)]))
                lowInterval, highInterval = intervalRange
                lowCyclesValue = (
                    previousElapsed + lowInterval) / baseLength
                lowCycles = int(lowCyclesValue)
                if lowCycles < lowCyclesValue - 1e-9:
                    lowCycles += 1
                highCycles = int(
                    (previousElapsed + highInterval + 1e-9) / baseLength)
                eventBudget = 256
                if method == 'stutter':
                    eventBudget -= len(calls) * (amount - 1)
                maxCycles = min(
                    int(4096.0 / baseLength),
                    max(0, eventBudget // max(1, eventCount)))
                highCycles = min(highCycles, maxCycles)
                if lowCycles <= highCycles:
                    targetCycles = max(
                        lowCycles, min(highCycles, candidateCycles))
                    adjustedInterval = (
                        targetCycles * baseLength - previousElapsed)
                    if (lowInterval - 1e-9 <= adjustedInterval <=
                            highInterval + 1e-9):
                        elapsed = targetCycles * baseLength
                        calls[-1] = elapsed
                        aligned = True
                        candidateCycles = targetCycles
            if aligned:
                elapsed = candidateCycles * baseLength
                calls[-1] = elapsed
                cycles = candidateCycles
                break
        if not calls or elapsed > 4096.0 or cycles < 1:
            raise ValueError('Scheduled operation could not form a bounded repeating phrase; use a shorter pattern or another interval.')
        if (method == 'stutter' and
                eventCount * cycles + len(calls) * (amount - 1) > 256):
            raise ValueError('Scheduled stutter expands beyond the 256-event limit; use fewer notes, fewer triggers, or a smaller count.')
        return {
            'method': method,
            'amount': amount,
            'calls': tuple(calls),
            'period': elapsed,
            'shuffleOrders': tuple(shuffleOrders),
            'closureAtPeriod': True,
        }

    def _apply_stacked_scheduled_transforms(self):
        pattern = self._currentPattern
        base = self._baseUpdate
        if pattern is None or base is None:
            raise ValueError('Assign a note pattern before scheduling player operations.')
        if pattern.synth == 'sample':
            raise ValueError('Sample players currently support scheduled stutter only.')
        if (pattern.octaveControl is not None or
                pattern.strumControl is not None or
                pattern.strumBeats != 0.0 or
                pattern.delayControl is not None or
                pattern.delayBeats != 0.0):
            raise ValueError('Scheduled player operations require fixed-octave, unstrummed notes without per-note delay.')

        def noteItems(value):
            if isinstance(value, _FoxDotChord):
                return [value]
            if isinstance(value, (list, tuple, _FoxDotFinitePattern)):
                return list(value)
            return [value]

        notes = noteItems(pattern.notes)
        if (not notes or len(notes) > 256 or
                any(isinstance(note, bool) or not isinstance(note, (int, float))
                    for note in notes)):
            raise ValueError('Scheduled player operations need a dense, monophonic numeric degree pattern.')
        events = list(base[3])
        if len(events) != len(notes):
            raise ValueError('Scheduled player operations need exactly one note event per pattern step.')
        byPosition = {}
        for event in events:
            position = event[8]
            if (isinstance(position, bool) or not isinstance(position, int) or
                    not 0 <= position < len(notes) or event[9] != 0 or
                    position in byPosition):
                raise ValueError('Scheduled player operations need one ungrouped note at each pattern step.')
            byPosition[position] = event
        if len(byPosition) != len(notes):
            raise ValueError('Scheduled player operations cannot move notes into rests.')

        baseLength = float(base[2])
        if not 0.0 < baseLength <= 4096.0:
            raise ValueError('Scheduled phrase length must be between 0 and 4096 beats.')
        operations = [
            self._compile_scheduled_operation(
                operation, baseLength, len(events))
            for operation in self._scheduledTransforms
        ]
        longestPeriod = max(operation['period'] for operation in operations)
        commonLength = None
        maxMultiplier = max(1, int(4096.0 / longestPeriod))
        for multiplier in range(1, maxMultiplier + 1):
            candidate = longestPeriod * multiplier
            if all(abs(candidate / operation['period'] -
                       round(candidate / operation['period'])) <= 1e-8
                   for operation in operations):
                commonLength = candidate
                break
        if commonLength is None:
            raise ValueError('Combined schedules do not form a repeating phrase within 4096 beats.')
        baseCycles = round(commonLength / baseLength)
        if (baseCycles < 1 or
                abs(commonLength - baseCycles * baseLength) > 1e-7):
            raise ValueError('Combined schedules do not align with the player pattern.')

        triggers = []
        for operationIndex, operation in enumerate(operations):
            localCycles = round(commonLength / operation['period'])
            method = operation['method']
            callLimit = (len(operation['calls']) - 1
                         if operation['closureAtPeriod'] and
                         method != 'stutter' else len(operation['calls']))
            for localCycle in range(localCycles):
                cycleOffset = localCycle * operation['period']
                for callIndex in range(callLimit):
                    beat = cycleOffset + operation['calls'][callIndex]
                    if beat >= commonLength - 1e-8:
                        if (operation['closureAtPeriod'] and
                                method == 'stutter' and
                                abs(beat - commonLength) <= 1e-8):
                            beat = 0.0
                        else:
                            continue
                    triggers.append((beat, operationIndex, callIndex))
                    if len(triggers) > 256:
                        raise ValueError('Combined schedules exceed 256 scheduled calls; lengthen intervals or use fewer operations.')
        triggers.sort(key=lambda item: (item[0], item[1], item[2]))
        stutterCalls = [
            trigger for trigger in triggers
            if operations[trigger[1]]['method'] == 'stutter'
        ]
        stutterExtraEvents = sum(
            operations[operationIndex]['amount'] - 1
            for _, operationIndex, _ in stutterCalls)
        if len(events) * baseCycles + stutterExtraEvents > 256:
            raise ValueError('Combined scheduled operations exceed the 256-event limit; use a shorter phrase, fewer calls, or smaller stutter counts.')

        states = []
        for operation in operations:
            if operation['method'] == 'reverse':
                states.append(False)
            elif operation['method'] == 'rotate':
                states.append(0)
            elif operation['method'] == 'shuffle':
                states.append(list(operation['shuffleOrders'][-1]))
            else:
                states.append(None)

        outputEvents = []
        triggerIndex = 0
        for cycleIndex in range(baseCycles):
            cycleOffset = cycleIndex * baseLength
            for position in range(len(notes)):
                event = byPosition[position]
                eventBeat = (cycleOffset + float(event[0]) +
                             float(event[6]))
                while (triggerIndex < len(triggers) and
                       triggers[triggerIndex][0] <= eventBeat + 1e-9):
                    _, operationIndex, callIndex = triggers[triggerIndex]
                    operation = operations[operationIndex]
                    method = operation['method']
                    if method == 'reverse':
                        states[operationIndex] = not states[operationIndex]
                    elif method == 'rotate':
                        states[operationIndex] = (
                            states[operationIndex] + operation['amount']
                        ) % len(notes)
                    elif method == 'shuffle':
                        states[operationIndex] = list(
                            operation['shuffleOrders'][callIndex])
                    triggerIndex += 1

                sourcePosition = position
                for operationIndex, operation in enumerate(operations):
                    method = operation['method']
                    if method == 'reverse' and states[operationIndex]:
                        sourcePosition = len(notes) - 1 - sourcePosition
                    elif method == 'rotate':
                        sourcePosition = (
                            sourcePosition - states[operationIndex]
                        ) % len(notes)
                    elif method == 'shuffle':
                        sourcePosition = states[operationIndex][sourcePosition]
                repeatedEvent = list(event)
                repeatedEvent[0] = float(event[0]) + cycleOffset
                repeatedEvent[2] = byPosition[sourcePosition][2]
                outputEvents.append(tuple(repeatedEvent))

        stepStarts = sorted(set(
            (float(event[0]) + float(event[6])) % baseLength
            for event in events))
        for beat, operationIndex, _ in stutterCalls:
            amount = operations[operationIndex]['amount']
            callBeat = beat % commonLength
            phase = callBeat % baseLength
            previousEvents = [
                event for event in outputEvents
                if float(event[0]) + float(event[6]) <= callBeat + 1e-9
            ]
            if previousEvents:
                event = previousEvents[-1]
            else:
                event = outputEvents[-1]
            nextStarts = [start for start in stepStarts
                          if start > phase + 1e-8]
            nextStart = (nextStarts[0] if nextStarts else
                         stepStarts[0] + baseLength)
            remaining = nextStart - phase
            if remaining <= 1e-9:
                continue
            spacing = remaining / float(amount)
            for repeatIndex in range(1, amount):
                repeatedEvent = list(event)
                repeatedStart = (
                    callBeat + spacing * repeatIndex) % commonLength
                repeatedEvent[0] = repeatedStart - float(event[6])
                repeatedEvent[1] = min(float(event[1]), spacing)
                outputEvents.append(tuple(repeatedEvent))
        outputEvents.sort(
            key=lambda event: float(event[0]) + float(event[6]))
        if len(outputEvents) > 256:
            raise ValueError('Combined scheduled operations exceed the 256-event limit.')
        expanded = list(base)
        expanded[2] = commonLength
        expanded[3] = outputEvents
        _dexter_updates[self.number - 1] = tuple(expanded)

    def _apply_scheduled_transform(self):
        pattern = self._currentPattern
        base = self._baseUpdate
        if pattern is None or base is None:
            raise ValueError('Assign a note pattern before scheduling a player transform.')
        if pattern.synth == 'sample':
            self._apply_sample_stutter(self._scheduledTransforms)
            return
        if (len(self._scheduledTransforms) > 1 or
                self._scheduledTransforms[0][5] is not None or
                self._scheduledTransforms[0][6] is not None):
            self._apply_stacked_scheduled_transforms()
            return
        if (pattern.octaveControl is not None or pattern.strumControl is not None or
                pattern.strumBeats != 0.0 or pattern.delayControl is not None or
                pattern.delayBeats != 0.0):
            raise ValueError('Scheduled player operations currently require fixed-octave, unstrummed notes without per-note delay.')

        def noteItems(value):
            if isinstance(value, _FoxDotChord):
                return [value]
            if isinstance(value, (list, tuple, _FoxDotFinitePattern)):
                return list(value)
            return [value]

        notes = noteItems(pattern.notes)
        if (not notes or len(notes) > 256 or
                any(isinstance(note, bool) or not isinstance(note, (int, float))
                    for note in notes)):
            raise ValueError('Scheduled player operations currently need a dense, monophonic numeric degree pattern (no rests or chords).')
        events = list(base[3])
        if len(events) != len(notes):
            raise ValueError('Scheduled player operations need exactly one note event per pattern step.')
        byPosition = {}
        for event in events:
            position = event[8]
            if (isinstance(position, bool) or not isinstance(position, int) or
                    not 0 <= position < len(notes) or event[9] != 0 or
                    position in byPosition):
                raise ValueError('Scheduled player operations currently need one ungrouped note at each pattern step.')
            byPosition[position] = event
        if len(byPosition) != len(notes):
            raise ValueError('Scheduled player operations cannot move notes into rests; use a dense degree pattern.')

        transform, amount, intervals, intervalRange = \
            self._scheduledTransforms[0][:4]
        baseLength = float(base[2])
        if not 0.0 < baseLength <= 4096.0:
            raise ValueError('Scheduled transform phrase length must be between 0 and 4096 beats.')
        calls = []
        elapsed = 0.0
        cycles = 0
        reverseState = False
        rotation = 0
        shuffleOrders = []
        for index in range(256):
            elapsed += float(intervals[index % len(intervals)])
            calls.append(elapsed)
            if elapsed > 4096.0:
                break
            if transform == 'reverse':
                reverseState = not reverseState
            elif transform == 'rotate':
                rotation = (rotation + amount) % len(notes)
            elif transform == 'shuffle':
                shuffleOrders.append(list(
                    PShuf(_FoxDotFinitePattern(list(range(len(notes)))))))
            candidateCycles = round(elapsed / baseLength)
            aligned = abs(elapsed - candidateCycles * baseLength) <= 1e-8
            reset = (
                not reverseState if transform == 'reverse' else
                rotation == 0 if transform == 'rotate' else
                True)
            if candidateCycles >= 1 and reset:
                if not aligned:
                    previousElapsed = elapsed - float(intervals[index % len(intervals)])
                    lowInterval, highInterval = intervalRange
                    lowCyclesValue = (previousElapsed + lowInterval) / baseLength
                    lowCycles = int(lowCyclesValue)
                    if lowCycles < lowCyclesValue - 1e-9:
                        lowCycles += 1
                    highCycles = int(
                        (previousElapsed + highInterval + 1e-9) / baseLength)
                    eventBudget = 256
                    if transform == 'stutter':
                        eventBudget -= len(calls) * (amount - 1)
                    maxCycles = min(
                        int(4096.0 / baseLength),
                        max(0, eventBudget // len(events)))
                    highCycles = min(highCycles, maxCycles)
                    if lowCycles <= highCycles:
                        targetCycles = max(
                            lowCycles, min(highCycles, candidateCycles))
                        adjustedInterval = (
                            targetCycles * baseLength - previousElapsed)
                        if lowInterval - 1e-9 <= adjustedInterval <= highInterval + 1e-9:
                            elapsed = targetCycles * baseLength
                            calls[-1] = elapsed
                            aligned = True
                            candidateCycles = targetCycles
                if aligned:
                    cycles = candidateCycles
                    break
        if (not calls or elapsed > 4096.0 or cycles < 1):
            raise ValueError('Scheduled transform could not form a bounded repeating phrase under the 256-event limit; use a shorter phrase or another interval.')
        scheduledExtraEvents = (
            len(calls) * (amount - 1) if transform == 'stutter' else 0)
        if len(events) * cycles + scheduledExtraEvents > 256:
            raise ValueError('Scheduled transform expands beyond the 256-event limit; use a shorter phrase or fewer note steps.')

        outputEvents = []
        callIndex = 0
        reverseState = False
        rotation = 0
        shuffleOrder = shuffleOrders[-1] if shuffleOrders else None
        for cycleIndex in range(cycles):
            cycleOffset = cycleIndex * baseLength
            for position in range(len(notes)):
                event = byPosition[position]
                eventBeat = cycleOffset + float(event[0])
                while callIndex < len(calls) and calls[callIndex] <= eventBeat + 1e-9:
                    if transform == 'reverse':
                        reverseState = not reverseState
                    elif transform == 'rotate':
                        rotation = (rotation + amount) % len(notes)
                    elif transform == 'shuffle':
                        shuffleOrder = shuffleOrders[callIndex]
                    callIndex += 1
                sourcePosition = (
                    len(notes) - 1 - position if reverseState else
                    (position - rotation) % len(notes) if transform == 'rotate' else
                    shuffleOrder[position] if transform == 'shuffle' else
                    position)
                repeatedEvent = list(event)
                repeatedEvent[0] = float(event[0]) + cycleOffset
                repeatedEvent[2] = byPosition[sourcePosition][2]
                outputEvents.append(tuple(repeatedEvent))

        if transform == 'stutter':
            stepStarts = sorted(
                ((float(event[0]), event) for event in byPosition.values()),
                key=lambda item: item[0])
            for scheduledBeat in calls:
                callBeat = scheduledBeat % elapsed
                phase = callBeat % baseLength
                selectedIndex = len(stepStarts) - 1
                for stepIndex, (stepBeat, _) in enumerate(stepStarts):
                    if stepBeat <= phase + 1e-9:
                        selectedIndex = stepIndex
                    else:
                        break
                stepBeat, event = stepStarts[selectedIndex]
                if phase + 1e-9 < stepBeat:
                    nextStepBeat = stepStarts[0][0] + baseLength
                    remaining = nextStepBeat - phase
                elif selectedIndex + 1 < len(stepStarts):
                    nextStepBeat = stepStarts[selectedIndex + 1][0]
                    remaining = nextStepBeat - phase
                else:
                    nextStepBeat = stepStarts[0][0] + baseLength
                    remaining = nextStepBeat - phase
                if remaining <= 1e-9:
                    remaining = baseLength / len(stepStarts)
                spacing = remaining / float(amount)
                for repeatIndex in range(1, amount):
                    repeatedEvent = list(event)
                    repeatedEvent[0] = (callBeat + spacing * repeatIndex) % elapsed
                    repeatedEvent[1] = min(float(event[1]), spacing)
                    outputEvents.append(tuple(repeatedEvent))
                    if len(outputEvents) > 256:
                        raise ValueError('Scheduled stutter expands beyond the 256-event limit; use fewer notes, fewer triggers, or a smaller stutter count.')
            outputEvents.sort(key=lambda event: float(event[0]))

        expanded = list(base)
        expanded[2] = elapsed
        expanded[3] = outputEvents
        _dexter_updates[self.number - 1] = tuple(expanded)

    def _set_scheduled_transform(self, method, amount, intervals,
                                 intervalRange=None, phase=0.0,
                                 repeatCycle=None, ident=None):
        if self._currentPattern is None or self._baseUpdate is None:
            raise ValueError('Assign a player pattern before scheduling a transform.')
        if self._follows is not None or self._followers:
            raise ValueError('Scheduled transforms and degree-follow relationships cannot be combined on the same player yet.')
        if (method in {'reverse', 'shuffle'} and
                (isinstance(amount, bool) or amount != 1)) or (
                method == 'rotate' and
                (isinstance(amount, bool) or not isinstance(amount, int) or
                 not -256 <= amount <= 256)) or (
                method == 'stutter' and
                (isinstance(amount, bool) or not isinstance(amount, int) or
                 not 1 <= amount <= 16)):
            raise ValueError('Rotate amount must be -256 to 256, stutter count 1 to 16, and reverse/shuffle take no amount.')
        if method not in {'reverse', 'rotate', 'shuffle', 'stutter'}:
            raise ValueError('Embedded scheduled transforms support reverse, rotate, shuffle, and stutter.')
        if intervalRange is None:
            intervalRange = (min(intervals), max(intervals))
        if repeatCycle is not None:
            if (isinstance(repeatCycle, bool) or
                    not isinstance(repeatCycle, (int, float)) or
                    repeatCycle != repeatCycle or
                    repeatCycle in (float('inf'), float('-inf')) or
                    not 0.05 <= repeatCycle <= 4096.0):
                raise ValueError('Schedule cycle must be a finite interval from 0.05 to 4096 beats.')
            repeatCycle = float(repeatCycle)
        phaseValues = (tuple(phase) if isinstance(phase, (tuple, list))
                       else (phase,))
        if not phaseValues:
            raise ValueError('A scheduled cycle needs at least one phase offset.')
        for phaseValue in phaseValues:
            if (isinstance(phaseValue, bool) or
                    not isinstance(phaseValue, (int, float)) or
                    phaseValue != phaseValue or
                    phaseValue in (float('inf'), float('-inf')) or
                    phaseValue < 0.0 or
                    (repeatCycle is not None and
                     phaseValue >= repeatCycle)):
                raise ValueError('Scheduled cycle offsets must be finite and within their cycle.')
        if ident is not None:
            if isinstance(ident, bool) or not isinstance(ident, (str, int)):
                raise ValueError('Schedule ident must be a short string or integer.')
            ident = str(ident)
            if (not ident or len(ident) > 64 or
                    any(not (character.isalnum() or character in '_-')
                        for character in ident)):
                raise ValueError('Schedule ident may contain only letters, digits, underscore, and hyphen.')
        existingIndex = None
        for index in range(len(self._scheduledTransforms)):
            if (self._scheduledTransforms[index][0] == method and
                    self._scheduledTransforms[index][6] == ident):
                existingIndex = index
                break
        operation = (method, amount, tuple(intervals), tuple(intervalRange),
                     tuple(float(value) for value in phaseValues),
                     repeatCycle, ident,
                     repeatCycle is not None)
        previous = (self._scheduledTransforms[existingIndex]
                    if existingIndex is not None else None)
        if existingIndex is None:
            self._scheduledTransforms.append(operation)
        else:
            self._scheduledTransforms[existingIndex] = operation
        try:
            self._apply_scheduled_transform()
        except Exception:
            if existingIndex is None:
                self._scheduledTransforms.pop()
            else:
                self._scheduledTransforms[existingIndex] = previous
            raise

    def every(self, occurrence, command, *args, **kwargs):
        cycle = kwargs.pop('cycle', None)
        ident = kwargs.pop('ident', None)
        if kwargs or len(args) > 1:
            raise ValueError('Embedded .every() accepts one transform amount and the optional cycle= and ident= arguments.')
        if command not in {'reverse', 'rotate', 'shuffle', 'stutter'}:
            raise ValueError("Embedded .every() supports 'reverse', 'rotate', 'shuffle', and 'stutter'.")
        if (isinstance(occurrence, bool) or
                not isinstance(occurrence, (int, float)) or
                occurrence != occurrence or occurrence in (float('inf'), float('-inf'))):
            raise ValueError('.every() occurrence must be a finite number.')
        amount = args[0] if args else (2 if command == 'stutter' else 1)
        if cycle is None:
            if not 0.05 <= occurrence <= 4096.0:
                raise ValueError('.every() interval must be from 0.05 to 4096 beats.')
            intervals = [float(occurrence)]
            intervalRange = (float(occurrence), float(occurrence))
            phase = (0.0,)
        else:
            if (isinstance(cycle, bool) or
                    not isinstance(cycle, (int, float)) or
                    cycle != cycle or cycle in (float('inf'), float('-inf')) or
                    not 0.05 <= cycle <= 4096.0):
                raise ValueError('.every() cycle must be a finite interval from 0.05 to 4096 beats.')
            if not 0.0 <= occurrence < cycle:
                raise ValueError('.every() cycle offset must be at least zero and less than cycle.')
            intervals = [float(cycle)]
            intervalRange = (float(cycle), float(cycle))
            phase = (float(occurrence),)
        self._set_scheduled_transform(
            command, amount, intervals, intervalRange,
            phase=phase, repeatCycle=cycle, ident=ident)
        return self

    def _random_transform_seed(self, command, ident):
        seed = (_dexter_shuffle_seed ^
                ((self.number * 2654435761) & 0xffffffff)) & 0xffffffff
        salt = str(command) + ':' + ('' if ident is None else str(ident))
        for index in range(len(salt)):
            seed = (seed ^ (
                ord(salt[index]) * (index + 1) * 16777619)) & 0xffffffff
        return seed

    def _random_transform_intervals(self, low, high, command, ident):
        seed = self._random_transform_seed(command, ident)
        intervals = [value / 2.0 for value in
                     PRand(low, high, length=256, seed=seed)]
        return intervals, (low / 2.0, high / 2.0)

    def _random_transform_phases(self, low, high, command, ident, cycle):
        seed = self._random_transform_seed(command, ident)
        return tuple((value / 2.0) % cycle for value in
                     PRand(low, high, length=4, seed=seed))

    def often(self, command, *args, **kwargs):
        ident = kwargs.pop('ident', None)
        cycle = kwargs.pop('cycle', None)
        if kwargs or len(args) > 1:
            raise ValueError('Embedded .often() accepts a transform, optional amount, ident=, and cycle=.')
        if command not in {'reverse', 'rotate', 'shuffle', 'stutter'}:
            raise ValueError("Embedded .often() supports 'reverse', 'rotate', 'shuffle', and 'stutter'.")
        amount = args[0] if args else (2 if command == 'stutter' else 1)
        if cycle is None:
            intervals, intervalRange = self._random_transform_intervals(
                1, 8, command, ident)
            phase = (0.0,)
        else:
            intervals = [float(cycle)]
            intervalRange = (float(cycle), float(cycle))
            phase = self._random_transform_phases(
                1, 8, command, ident, float(cycle))
        self._set_scheduled_transform(
            command, amount, intervals, intervalRange,
            phase=phase, repeatCycle=cycle, ident=ident)
        return self

    def sometimes(self, command, *args, **kwargs):
        ident = kwargs.pop('ident', None)
        cycle = kwargs.pop('cycle', None)
        if kwargs or len(args) > 1:
            raise ValueError('Embedded .sometimes() accepts a transform, optional amount, ident=, and cycle=.')
        if command not in {'reverse', 'rotate', 'shuffle', 'stutter'}:
            raise ValueError("Embedded .sometimes() supports 'reverse', 'rotate', 'shuffle', and 'stutter'.")
        amount = args[0] if args else (2 if command == 'stutter' else 1)
        if cycle is None:
            intervals, intervalRange = self._random_transform_intervals(
                8, 32, command, ident)
            phase = (0.0,)
        else:
            intervals = [float(cycle)]
            intervalRange = (float(cycle), float(cycle))
            phase = self._random_transform_phases(
                8, 32, command, ident, float(cycle))
        self._set_scheduled_transform(
            command, amount, intervals, intervalRange,
            phase=phase, repeatCycle=cycle, ident=ident)
        return self

    def rarely(self, command, *args, **kwargs):
        ident = kwargs.pop('ident', None)
        cycle = kwargs.pop('cycle', None)
        if kwargs or len(args) > 1:
            raise ValueError('Embedded .rarely() accepts a transform, optional amount, ident=, and cycle=.')
        if command not in {'reverse', 'rotate', 'shuffle', 'stutter'}:
            raise ValueError("Embedded .rarely() supports 'reverse', 'rotate', 'shuffle', and 'stutter'.")
        amount = args[0] if args else (2 if command == 'stutter' else 1)
        if cycle is None:
            intervals, intervalRange = self._random_transform_intervals(
                32, 64, command, ident)
            phase = (0.0,)
        else:
            intervals = [float(cycle)]
            intervalRange = (float(cycle), float(cycle))
            phase = self._random_transform_phases(
                32, 64, command, ident, float(cycle))
        self._set_scheduled_transform(
            command, amount, intervals, intervalRange,
            phase=phase, repeatCycle=cycle, ident=ident)
        return self

    def _copy_followed_degree(self, source):
        if source._currentPattern is None or self._currentPattern is None:
            return
        if (source._currentPattern.synth == 'sample' or
                self._currentPattern.synth == 'sample'):
            raise ValueError('Player degree following is for numeric note patterns, not sample players.')
        if self._scheduledTransforms or source._scheduledTransforms:
            raise ValueError('Scheduled transforms and degree-follow relationships cannot be combined on the same player yet.')
        def noteItems(pattern):
            notes = pattern.notes
            if isinstance(notes, _FoxDotChord):
                return [notes]
            if isinstance(notes, (list, tuple, _FoxDotFinitePattern)):
                return list(notes)
            return [notes]

        sourceNotes = noteItems(source._currentPattern)
        targetNotes = noteItems(self._currentPattern)
        if not sourceNotes or not targetNotes or len(sourceNotes) > 256:
            raise ValueError('Player degree following needs non-empty patterns with at most 256 source steps.')
        clone = PlayerPattern.__new__(PlayerPattern)
        clone.__dict__ = dict(self._currentPattern.__dict__)
        clone.notes = [sourceNotes[index % len(sourceNotes)]
                       for index in range(len(targetNotes))]
        clone.degrees = source._currentPattern.degrees
        self._currentPattern = clone
        self._applyingFollow = True
        try:
            self.__rshift__(clone)
        finally:
            self._applyingFollow = False

    def _refresh_followers(self):
        for follower in list(self._followers):
            follower._copy_followed_degree(self)

    def follow(self, other=False):
        if not isinstance(other, Player) or not 1 <= other.number <= 8:
            raise TypeError('Player.follow requires one player from p1 through p8.')
        if other is self:
            raise ValueError('A player cannot follow itself.')
        if self._scheduledTransforms or other._scheduledTransforms:
            raise ValueError('Scheduled transforms and degree-follow relationships cannot be combined on the same player yet.')
        if (self._currentPattern is not None and
                other._currentPattern is not None and
                (self._currentPattern.synth == 'sample' or
                 other._currentPattern.synth == 'sample')):
            raise ValueError('Player degree following is for numeric note patterns, not sample players.')
        cursor = other
        while cursor is not None:
            if cursor is self:
                raise ValueError('Circular player degree reference.')
            cursor = cursor._follows
        previous = self._follows
        if previous is not None and self in previous._followers:
            previous._followers.remove(self)
        self._follows = other
        if self not in other._followers:
            other._followers.append(self)
        self._copy_followed_degree(other)
        return self

    def stop(self):
        _dexter_effect_control_variables[self.number - 1] = ('-', '-', '-', '-', '-', '-', '-')
        _dexter_updates[self.number - 1] = (True, _dexter_bpm, 1.0, [], 0, 1.0,
                                             0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-', 0.0, 0.0, None, 0.0, None, _dexter_swing_seconds)
        self._active = False
        return self

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

_DEXTER_PLAYERS = (p1, p2, p3, p4, p5, p6, p7, p8)

class Group:
    def __init__(self, *players):
        if len(players) > 8:
            raise ValueError('Group is limited to eight players.')
        if any(not isinstance(player, Player) or
               not 1 <= player.number <= 8 for player in players):
            raise TypeError('Group accepts only players p1 through p8.')
        if len(set(player.number for player in players)) != len(players):
            raise ValueError('Group cannot contain the same player twice.')
        self.__dict__['players'] = list(players)

    def __len__(self):
        return len(self.players)

    def __iter__(self):
        return iter(self.players)

    def __getattr__(self, name):
        if name in _GROUP_PATTERN_FIELDS:
            fieldName = _GROUP_PATTERN_FIELDS[name]
            values = []
            for player in self.players:
                pattern = player._currentPattern
                if pattern is None:
                    values.append(None)
                elif name == 'degree':
                    if pattern.degreeControl is not None:
                        values.append(pattern.degreeControl)
                    else:
                        shift = ((pattern.baseOctave - 5) *
                                 pattern.degreesPerOctave)
                        values.append(_group_shift_notes(
                            pattern.notes, -shift) if pattern.degrees else
                            pattern.notes)
                elif name == 'oct':
                    values.append(pattern.octaveControl
                                  if pattern.octaveControl is not None else
                                  pattern.baseOctave)
                elif name == 'root':
                    values.append(pattern.rootControl
                                  if pattern.rootControl is not None else
                                  pattern.rootMidi)
                else:
                    values.append(pattern.__dict__.get(fieldName))
            return values
        raise AttributeError('unsupported Group attribute ' + name)

    def __setattr__(self, name, value):
        if name not in _GROUP_PATTERN_FIELDS:
            raise AttributeError('Group assignments support shared sound/effect and pitch/timing controls.')
        value = _normalize_group_control(name, value)
        prepared = []
        for player in self.players:
            if player._currentPattern is not None:
                copiedPattern = PlayerPattern.__new__(PlayerPattern)
                copiedPattern.__dict__ = dict(player._currentPattern.__dict__)
                _set_group_pattern_control(copiedPattern, name, value)
                prepared.append((player, copiedPattern))
        for player in self.players:
            player._groupOverrides[name] = value
        for player, pattern in prepared:
            player.__rshift__(pattern)
        return self

    def add(self, player):
        if not isinstance(player, Player) or not 1 <= player.number <= 8:
            raise TypeError('Group.add accepts only a player from p1 through p8.')
        if player.number in [item.number for item in self.players]:
            raise ValueError('Group already contains that player.')
        if len(self.players) >= 8:
            raise ValueError('Group is limited to eight players.')
        self.players.append(player)
        return self

    def stop(self):
        for player in self.players:
            player.stop()
        return self

    def mute(self):
        for player in self.players:
            player.mute()
        return self

    def unmute(self):
        for player in self.players:
            player.unmute()
        return self

    def solo(self, enabled=True):
        if not isinstance(enabled, bool):
            raise TypeError('Group.solo accepts a boolean.')
        selected = set(player.number - 1 for player in self.players)
        for index in range(8):
            _dexter_mutes[index] = bool(enabled and index not in selected)
        return self

    def only(self):
        selected = set(player.number for player in self.players)
        for player in _DEXTER_PLAYERS:
            if player.number not in selected and player._active:
                player.stop()
        return self

    def _schedule(self, method, *args, **kwargs):
        for player in self.players:
            if method == 'every':
                player.every(*args, **kwargs)
            elif method == 'often':
                player.often(*args, **kwargs)
            elif method == 'sometimes':
                player.sometimes(*args, **kwargs)
            elif method == 'rarely':
                player.rarely(*args, **kwargs)
        return self

    def every(self, *args, **kwargs):
        return self._schedule('every', *args, **kwargs)

    def often(self, *args, **kwargs):
        return self._schedule('often', *args, **kwargs)

    def sometimes(self, *args, **kwargs):
        return self._schedule('sometimes', *args, **kwargs)

    def rarely(self, *args, **kwargs):
        return self._schedule('rarely', *args, **kwargs)

def Master():
    return Group(*(player for player in _DEXTER_PLAYERS if player._active))

p_all = Group(*_DEXTER_PLAYERS)

class _PlayerDegreeReference:
    def __init__(self, player):
        self.player = player

# Explicit VST extensions, not claims of upstream FoxDot Buffer/sclang APIs.
def _disk_number(value, name, lo, hi):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(name + ' requires a finite scalar number')
    value = float(value)
    if value != value or not lo <= value <= hi:
        raise ValueError(name + ' is outside the supported range')
    return value

def _disk_slot(slot):
    if isinstance(slot, bool) or not isinstance(slot, int) or not 1 <= slot <= 8:
        raise ValueError('Disk stream slot must be a whole number from 1 to 8')
    return slot - 1

def _disk_command(command):
    if len(_dexter_disk_commands) >= 16:
        raise ValueError('One Run may contain at most 16 disk lifecycle operations')
    _dexter_disk_commands.append(command)

class _DiskReader:
    def __init__(self, mode):
        self.mode = mode
    def play(self, path, slot=1, loop=False, amp=0.5, pan=0.0, rate=1.0):
        if not isinstance(path, str) or not 1 <= len(path) <= 512:
            raise ValueError('Disk input needs a relative file path under Portable/Samples')
        normalized = path.replace('\\', '/')
        if (normalized.startswith('/') or ':' in normalized or
                any(part in ('', '.', '..') for part in normalized.split('/')) or
                any(ord(character) < 32 for character in normalized)):
            raise ValueError('Disk input cannot be absolute, escape Portable/Samples, or contain control characters')
        if not isinstance(loop, bool):
            raise TypeError('Disk loop must be True or False')
        rate = _disk_number(rate, 'Disk rate', 0.0625, 8.0)
        if self.mode == 1 and rate != 1.0:
            raise ValueError('DiskIn has no rate control; use VDiskIn.play(..., rate=...)')
        _disk_command((self.mode, _disk_slot(slot), rate,
                       _disk_number(amp, 'Disk amp', 0.0, 2.0),
                       _disk_number(pan, 'Disk pan', -1.0, 1.0), 0.0,
                       int(loop), normalized.encode('utf-8').hex()))
    def stop(self, slot=1):
        _disk_command((0, _disk_slot(slot), 1.0, 0.0, 0.0, 0.0, 0, '-'))

class _DiskRecorder:
    def record(self, seconds=30.0):
        _disk_command((3, 0, 1.0, 0.0, 0.0,
                       _disk_number(seconds, 'Recording seconds', 0.05, 3600.0), 0, '-'))
    def stop(self):
        _disk_command((4, 0, 1.0, 0.0, 0.0, 0.0, 0, '-'))

DiskIn = _DiskReader(1)
VDiskIn = _DiskReader(2)
DiskOut = _DiskRecorder()
)PY";

    std::ostringstream script;
    script << "import builtins, traceback, sys, time\n"
           << "_dexter_scope = globals().setdefault(" << pythonLiteral(namespaceName) << ", {})\n"
           << "_dexter_scope['__name__'] = 'dexter'\n"
           << "_dexter_scope.update({'_dexter_events': [], '_dexter_bpm': 120.0, "
           << "'_dexter_length': 1.0, '_dexter_slots': 0, '_dexter_updates': {}, '_dexter_default_touched': False, "
             << "'_dexter_scale': (0, 2, 4, 5, 7, 9, 11), '_dexter_root': 60, "
             << "'_dexter_root_control': None, '_dexter_mutes': {}, "
            << "'_dexter_swing_seconds': 0.0, '_dexter_disk_commands': [], "
           << "'_dexter_shuffle_seed': int(time.monotonic_ns() & 0xffffffff)})\n"
           << "_dexter_scope['__builtins__'] = {name: getattr(builtins, name) for name in "
           << "('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter', 'float', "
           << "'int', 'isinstance', 'len', 'list', 'map', 'max', 'min', 'range', 'ord', 'chr', "
           << "'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum', 'tuple', "
           << "'zip', 'ValueError', 'TypeError', 'ZeroDivisionError', "
            << "'AttributeError', 'Exception', '__build_class__')}\n"
           << "exec(compile(" << pythonLiteral(api) << ", '<Dexter API>', 'exec'), "
            << "_dexter_scope, _dexter_scope)\n"
            << "_dexter_scope['_dexter_user_synth_names'] = "
            << pythonStringTuple(userSynthNames) << "\n"
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
            << "        _f.write('OK\\t{}\\t{}\\t{}\\n'.format(len(_updates), len(_mutes), len(_dexter_scope['_dexter_disk_commands'])))\n"
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
             << "            _effect_user_index = _dexter_scope['_dexter_player_effect_indices'].get(_player, -1)\n"
             << "            _f.write('A' + chr(9) + str(_player) + chr(9) + str(_effect_user_index) + chr(10))\n"
             << "            _effect_control_variables = _dexter_scope['_dexter_effect_control_variables'].get(_player, ('-', '-', '-', '-', '-', '-', '-'))\n"
             << "            _f.write('X' + chr(9) + str(_player) + chr(9) + chr(9).join(_effect_control_variables) + chr(10))\n"
             << "            if _synth == 57:\n"
             << "                _sample_control, _rate_control = _dexter_scope['_dexter_sample_controls'].get(_player, ('-', '-'))\n"
             << "                _f.write('Q' + chr(9) + str(_player) + chr(9) + _sample_control + chr(9) + _rate_control + chr(10))\n"
            << "            for _event_index, _e in enumerate(_events):\n"
            << "                _f.write('N\\t{}\t{}\t{}\t{}\t{}\t{}\\t{}\\n'.format(*_e))\n"
            << "                if _synth == 57:\n"
            << "                    _f.write('B\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\n'.format(*_dexter_scope['_dexter_sample_events'][_player][_event_index]))\n"
            << "        for _player, _muted in sorted(_mutes.items()):\n"
            << "            _f.write('M\\t{}\\t{}\\n'.format(_player, int(_muted)))\n"
            << "        for _command in _dexter_scope['_dexter_disk_commands']:\n"
            << "            _f.write('D' + chr(9) + chr(9).join(map(str, _command)) + chr(10))\n"
            << "except BaseException:\n"
           << "    with open(" << pythonLiteral(resultPath) << ", 'w', encoding='utf-8') as _f:\n"
           << "        _f.write('ERROR\\n' + traceback.format_exc())\n";
    return script.str();
}

bool parseResult(const std::filesystem::path& resultPath,
                 PythonPattern& pattern,
                 std::size_t userSynthCount,
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
    std::size_t diskCount = 0;
    fields >> std::ws;
    if (!fields.eof() && (!(fields >> diskCount) || diskCount > 16)) {
        error = "Python returned an invalid DiskIO command count."; return false;
    }
    pattern.diskCommands.clear();
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
               update.lengthBeats > 4096.0 || update.synthIndex < 0 ||
               static_cast<std::size_t>(update.synthIndex) >=
                    kEmbeddedSynthCount + userSynthCount ||
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
        char playerEffectRecord = 0;
        int playerEffectPlayer = -1;
        if (!(input >> playerEffectRecord >> playerEffectPlayer >>
              update.playerEffectUserIndex) ||
            playerEffectRecord != 'A' ||
            playerEffectPlayer != update.playerIndex ||
            update.playerEffectUserIndex < -1 ||
            (update.playerEffectUserIndex >= 0 &&
             static_cast<std::size_t>(update.playerEffectUserIndex) >=
                 userSynthCount)) {
            error = "Python returned an invalid per-player Airwindows effect selection.";
            return false;
        }
        char effectControlsRecord = 0;
        int effectControlsPlayer = -1;
        if (!(input >> effectControlsRecord >> effectControlsPlayer) ||
            effectControlsRecord != 'X' ||
            effectControlsPlayer != update.playerIndex) {
            error = "Python returned malformed Airwindows effect controls.";
            return false;
        }
        const auto validEffectControlToken = [](const std::string& token) {
            if (token == "-")
                return true;
            if (token.size() > 256 || token.size() < 5 ||
                (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                return false;
            const auto middle = token.find(':', 2);
            if (middle == std::string::npos || middle == 2 ||
                middle + 1 >= token.size() ||
                token.find(':', middle + 1) != std::string::npos)
                return false;
            const auto validNumberList = [](const std::string& text, double minimum,
                                            double maximum, bool positive,
                                            std::size_t& count) {
                if (text.empty() || text.front() == ',' || text.back() == ',' ||
                    text.find(",,") != std::string::npos)
                    return false;
                std::istringstream values(text);
                std::string part;
                count = 0;
                while (std::getline(values, part, ',')) {
                    if (part.empty() || count >= 8)
                        return false;
                    try {
                        std::size_t used = 0;
                        const double value = std::stod(part, &used);
                        if (used != part.size() || !std::isfinite(value) ||
                            (positive ? value <= 0.0 || value > maximum
                                      : value < minimum || value > maximum))
                            return false;
                    } catch (...) {
                        return false;
                    }
                    ++count;
                }
                return count > 0;
            };
            std::size_t valueCount = 0;
            std::size_t durationCount = 0;
            return validNumberList(token.substr(2, middle - 2), 0.0, 1.0,
                                  false, valueCount) &&
                   validNumberList(token.substr(middle + 1), 0.0, 4096.0,
                                   true, durationCount) &&
                   (durationCount == 1 || durationCount == valueCount);
        };
        for (auto& token : update.effectControlTokens) {
            if (!(input >> token) || !validEffectControlToken(token)) {
                error = "Python returned malformed Airwindows effect-control modulation.";
                return false;
            }
            if (token != "-" && update.playerEffectUserIndex < 0) {
                error = "Python returned Airwindows effect modulation without an effect selection.";
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
                int loop = 0;
                if (!(input >> bufferRecord >> event.sampleSymbol >> event.sampleIndex >>
                      event.sampleRate >> gate >> event.sampleStart >> event.sampleEnd >>
                      loop) || bufferRecord != 'B' ||
                    event.sampleSymbol < 33 || event.sampleSymbol > 126 ||
                    event.sampleIndex < -1000000 || event.sampleIndex > 1000000 ||
                    !std::isfinite(event.sampleRate) ||
                    std::abs(event.sampleRate) < 0.0625 || std::abs(event.sampleRate) > 8.0 ||
                    (gate != 0 && gate != 1) ||
                    !std::isfinite(event.sampleStart) ||
                    !std::isfinite(event.sampleEnd) ||
                    event.sampleStart < 0.0 || event.sampleEnd > 1.0 ||
                    event.sampleStart >= event.sampleEnd ||
                    (loop != 0 && loop != 1)) {
                    error = "Python returned a malformed sample event.";
                    return false;
                }
                event.sampleGate = gate != 0;
                event.sampleLoop = loop != 0;
                if (event.sampleLoop && !event.sampleGate) {
                    error = "Python returned a looping sample without a bounded gate.";
                    return false;
                }
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
    for (std::size_t i = 0; i < diskCount; ++i) {
        PythonDiskCommand command;
        char record = 0;
        int loop = 0;
        std::string pathHex;
        if (!(input >> record >> command.mode >> command.slot >> command.rate >>
              command.amp >> command.pan >> command.seconds >> loop >> pathHex) ||
            record != 'D' || command.mode < 0 || command.mode > 4 ||
            command.slot < 0 || command.slot > 7 || (loop != 0 && loop != 1) ||
            !std::isfinite(command.rate) || command.rate < 0.0625 || command.rate > 8 ||
            !std::isfinite(command.amp) || command.amp < 0 || command.amp > 2 ||
            !std::isfinite(command.pan) || command.pan < -1 || command.pan > 1 ||
            !std::isfinite(command.seconds) ||
            (command.mode == 3 && (command.seconds < .05 || command.seconds > 3600)) ||
            (command.mode != 3 && command.seconds != 0)) {
            error = "Python returned an invalid DiskIO command."; return false;
        }
        command.loop = loop != 0;
        if (command.mode == 1 || command.mode == 2) {
            if (pathHex.empty() || pathHex.size() > 4096 || pathHex.size() % 2) {
                error = "Python returned an invalid disk input path."; return false;
            }
            auto digit = [](char c) {
                return c >= '0' && c <= '9' ? c - '0' :
                       c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            };
            for (std::size_t pos = 0; pos < pathHex.size(); pos += 2) {
                const auto high = digit(pathHex[pos]), low = digit(pathHex[pos + 1]);
                if (high < 0 || low < 0 || high * 16 + low < 32) {
                    error = "Python returned invalid disk path encoding."; return false;
                }
                command.path.push_back(static_cast<char>(high * 16 + low));
            }
        } else if (pathHex != "-") {
            error = "Python returned a path for a non-reading disk command."; return false;
        }
        pattern.diskCommands.push_back(std::move(command));
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
                              const std::vector<std::string>& userSynthNames,
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
    const auto script = makeEvaluationScript(sourceUtf8, resultUtf8, namespaceName,
                                             userSynthNames);
    int result = -1;
    {
        std::lock_guard<std::mutex> lock(gPythonMutex);
        const int gil = gPython.gilStateEnsure();
        result = gPython.runSimpleStringFlags(script.c_str(), nullptr);
        gPython.gilStateRelease(gil);
    }

    bool success = false;
    if (result == 0)
        success = parseResult(resultPath, pattern, userSynthNames.size(), error);
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
                                       const std::vector<std::string>& userSynthNames,
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
                                                pathAsUtf8(reportPath),
                                                userSynthNames);
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
