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
    'tuple', 'zip', 'ValueError', 'TypeError', 'IndexError', 'OverflowError',
    'ZeroDivisionError',
    'tempo', 'scale', 'reverse', 'rotate', 'repeat', 'palindrome', 'stutter',
    'interleave', 'transpose', 'euclid', 'PAlt', 'PStretch', 'PStutter',
    'PShuf', 'PJoin', 'PBeat', 'PDur', 'PDelay', 'PStrum', 'PQuicken', 'PRhythm',
    'PZip', 'PZip2', 'PRand', 'PWhite', 'PWalk', 'PChain', 'PDelta', 'Pvar',
    'PSquare', 'PSq', 'PRange', 'PStep', 'PEuclid', 'PEuclid2',
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
_COMPAT_PATTERN_METHODS = {
    'reverse', 'rotate', 'mirror', 'accum', 'swap', 'repeat', 'duplicate',
    'iter', 'stretch',
    'loop', 'shuffle', 'deep_shuffle', 'sample', 'true_shuffle', 'shufflets',
    'invert', 'arp', 'splice',
    'sort', 'unduplicate', 'undup', 'replace', 'trim', 'ltrim', 'compress',
    'select', 'normalise',
    'norm', 'choose', 'zip', 'zipx', 'deepzip', 'deeprzip', 'map', 'pivot', 'layer',
    'stutter', 'palindrome', 'expand', 'count', 'index', 'submap', 'concat',
    'add', 'abs', 'all', 'startswith', 'limit', 'every', 'copy',
    'asGroup', 'group', 'int', 'float', 'str', 'append', 'extend',
    'insert', 'pop', 'remove', 'set', 'alt', 'string', 'items', 'getitem',
    'getslice', 'setitem', 'eq', 'ne', 'i_rotate', 'i_reverse', 'i_sort',
    'i_shuf'
}
_COMPAT_PATTERN_CALLS = {
    'PAlt', 'PStretch', 'PStutter', 'PShuf', 'PJoin', 'PBeat', 'PDur',
    'PDelay', 'PStrum', 'PQuicken', 'PRhythm', 'PZip', 'PZip2', 'PRand',
    'PWhite', 'PWalk', 'PChain', 'PDelta', 'Pvar', 'PSquare', 'PSq',
    'PRange', 'PStep', 'PEuclid', 'PEuclid2', 'reverse', 'rotate', 'repeat',
    'palindrome', 'stutter', 'interleave', 'transpose', 'euclid'
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
    'vib', 'depth', 'vibdepth', 'slide', 'slidefrom', 'slidedelay',
    'bend', 'benddelay', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur',
    'shape', 'drive', 'chop', 'spin', 'effect', 'fx_vib', 'fx_depth',
    'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60',
    'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq',
    'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone'
}
_USER_SYNTH_KEYWORDS = {
    'notes', 'dur', 'oct', 'sus', 'scale', 'root', 'amp', 'pan', 'cutoff',
    'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'velocity',
    'rate', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime',
    'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell',
    'strum', 'vib', 'depth', 'vibdepth', 'slide', 'slidefrom', 'slidedelay',
    'bend', 'benddelay', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur',
    'shape', 'drive', 'chop', 'spin', 'effect', 'fx_vib', 'fx_depth',
    'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60',
    'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq',
    'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone'
}
_COMPAT_SEQUENCE_KEYWORDS = {
    'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'synth',
    'amp', 'pan', 'cutoff', 'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell', 'root', 'strum', 'rate',
    'shape', 'drive', 'vib', 'depth', 'vibdepth', 'slide', 'slidefrom',
    'slidedelay', 'bend', 'benddelay', 'chop', 'spin', 'effect',
    'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq',
    'fx_blur', 'fx_mix', 'fx_t60', 'fx_damp', 'fx_size',
    'fx_earlydiff', 'fx_moddepth', 'fx_modfreq', 'fx_delay', 'fx_diff',
    'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone'
}
_COMPAT_NOTE_PLAY_KEYWORDS = {'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'strum', 'shape', 'drive', 'chop', 'spin', 'effect', 'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60', 'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq', 'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone'}
_COMPAT_SAMPLE_PLAY_KEYWORDS = (_COMPAT_SEQUENCE_KEYWORDS - {'notes', 'step', 'duration', 'degrees', 'synth', 'root', 'strum'}) | {'dur', 'sus', 'sample', 'start', 'end', 'loop'}
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
    'degree', 'dur', 'sus', 'oct', 'scale', 'root', 'shape', 'drive',
    'chop', 'spin'
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
        self.pattern_names = set()
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
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == 'Pvar':
            values = (node.args[0] if node.args else next(
                (keyword.value for keyword in node.keywords
                 if keyword.arg == 'patterns'), None))
            if values is None:
                return None
            patterns = (values.elts if isinstance(values, (ast.List, ast.Tuple))
                        else [values])
            kinds = {kind for kind in (self._pattern_kind(value) for value in patterns)
                     if kind is not None}
            if 'mixed' in kinds or len(kinds) > 1:
                return 'mixed'
            return next(iter(kinds)) if kinds else None
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            if node.func.id in {'PRange', 'PSq', 'PEuclid'}:
                return 'numeric'
            if node.func.id in {'PStep', 'PEuclid2'}:
                if node.func.id == 'PStep':
                    value = (node.args[1] if len(node.args) > 1 else next(
                        (keyword.value for keyword in node.keywords
                         if keyword.arg == 'value'), None))
                    default = (node.args[2] if len(node.args) > 2 else next(
                        (keyword.value for keyword in node.keywords
                         if keyword.arg == 'default'), ast.Constant(0)))
                    candidates = (value, default)
                else:
                    low = (node.args[2] if len(node.args) > 2 else next(
                        (keyword.value for keyword in node.keywords
                         if keyword.arg == 'lo'), None))
                    high = (node.args[3] if len(node.args) > 3 else next(
                        (keyword.value for keyword in node.keywords
                         if keyword.arg == 'hi'), None))
                    candidates = (low, high)
                kinds = {kind for kind in (
                    self._pattern_kind(value) for value in candidates)
                         if kind is not None}
                if 'mixed' in kinds or len(kinds) > 1:
                    return 'mixed'
                return next(iter(kinds)) if kinds else None
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
            return self._pattern_kind(node.func.value)
        return None

    def _finite_pattern_expression(self, node):
        if isinstance(node, ast.Name):
            return node.id in self.pattern_names
        if isinstance(node, ast.Subscript) and isinstance(
                node.value, ast.Name) and node.value.id == 'P':
            return self._pattern_kind(node) is not None
        if isinstance(node, ast.BinOp) and isinstance(
                node.op, (ast.BitAnd, ast.BitOr)):
            return (self._finite_pattern_expression(node.left) and
                    self._finite_pattern_expression(node.right))
        if isinstance(node, ast.Call):
            if isinstance(node.func, ast.Name):
                return node.func.id in _COMPAT_PATTERN_CALLS
            if (isinstance(node.func, ast.Attribute) and
                    node.func.attr in _COMPAT_PATTERN_METHODS):
                return self._finite_pattern_expression(node.func.value)
        return False

    def _check_pattern_kinds(self, nodes, line, helper_name):
        kinds = {self._pattern_kind(node) for node in nodes}
        kinds.discard(None)
        if 'mixed' in kinds or len(kinds) > 1:
            self.add(line, helper_name +
                     ' cannot mix numeric note patterns with sample-symbol patterns.')

    def _check_fixed_signature(self, node, function_name, parameters,
                               required_count):
        assigned = set(parameters[:len(node.args)])
        if len(node.args) > len(parameters):
            self.add(node.lineno, function_name + ' accepts at most ' +
                     str(len(parameters)) + ' positional arguments.')
        for keyword in node.keywords:
            if keyword.arg is None:
                self.add(keyword.lineno or node.lineno,
                         function_name + ' does not accept dynamic keyword expansion.')
            elif keyword.arg not in parameters:
                self.add(keyword.lineno or node.lineno, function_name +
                         ' does not support keyword ' + keyword.arg + '.')
            elif keyword.arg in assigned:
                self.add(keyword.lineno or node.lineno, function_name +
                         ' received ' + keyword.arg + ' more than once.')
            else:
                assigned.add(keyword.arg)
        missing = [parameter for parameter in parameters[:required_count]
                   if parameter not in assigned]
        if missing:
            self.add(node.lineno, function_name + ' requires ' +
                     ', '.join(missing) + '.')

    def visit_Import(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_ImportFrom(self, node):
        self.add(node.lineno, 'Python imports are disabled in the portable editor; use the built-in API.')

    def visit_Call(self, node):
        name = node.func.id if isinstance(node.func, ast.Name) else None
        if (isinstance(node.func, ast.Attribute) and
                self._finite_pattern_expression(node.func.value) and
                node.func.attr not in _COMPAT_PATTERN_METHODS):
            self.add(node.lineno, "P pattern method '" + node.func.attr +
                     "()' is not in the embedded API; use a supported P method or built-in helper.")
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
        if name == 'PRange':
            self._check_fixed_signature(node, name, ('start', 'stop', 'step'), 1)
        if name == 'PSq':
            self._check_fixed_signature(node, name, ('a', 'b', 'c'), 0)
        if name == 'PStep':
            self._check_fixed_signature(node, name, ('n', 'value', 'default'), 2)
        if name == 'PEuclid':
            self._check_fixed_signature(node, name, ('n', 'k'), 2)
        if name == 'PEuclid2':
            self._check_fixed_signature(node, name, ('n', 'k', 'lo', 'hi'), 4)
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
        if name == 'Pvar':
            parameters = ('patterns', 'dur')
            allowed = set(parameters)
            if len(node.args) > len(parameters):
                self.add(node.lineno, 'Pvar accepts a pattern list and optional dur.')
            positional = set(parameters[:len(node.args)])
            for keyword in node.keywords:
                if keyword.arg is None or keyword.arg not in allowed:
                    self.add(node.lineno, 'Pvar accepts only patterns and dur.')
                elif keyword.arg in positional:
                    self.add(keyword.lineno or node.lineno,
                             'Pvar received the same argument more than once.')
                else:
                    positional.add(keyword.arg)
            if 'patterns' not in positional:
                self.add(node.lineno, 'Pvar requires a finite list of phrase patterns.')
            if node.args:
                phrases = (node.args[0].elts if isinstance(
                    node.args[0], (ast.List, ast.Tuple)) else [node.args[0]])
            else:
                phraseArgument = next(
                    (keyword.value for keyword in node.keywords
                     if keyword.arg == 'patterns'), None)
                phrases = (phraseArgument.elts if isinstance(
                    phraseArgument, (ast.List, ast.Tuple)) else
                    [phraseArgument] if phraseArgument is not None else [])
            self._check_pattern_kinds(phrases, node.lineno, 'Pvar')
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
        if name in _COMPAT_WAVES or name in {'play', 'sequence', 'userSynth'}:
            for keyword in node.keywords:
                if keyword.arg != 'effect':
                    continue
                if not isinstance(keyword.value, ast.Constant) or not isinstance(keyword.value.value, str):
                    self.add(keyword.lineno or node.lineno,
                             'effect= requires a literal installed player-effect name.')
                elif keyword.value.value not in {'AirwindowsTapeDelay2', 'AirwindowsEchojam', 'JPverb', 'Greyhole', 'Chorus', 'Flanger'}:
                    self.add(keyword.lineno or node.lineno,
                             'effect= supports AirwindowsTapeDelay2, AirwindowsEchojam, JPverb, Greyhole, Chorus or Flanger.')
                elif keyword.value.value not in _USER_SYNTH_NAMES:
                    self.add(keyword.lineno or node.lineno,
                             'effect= requires the matching player-effect SynthDef in Portable/SynthDefs.')
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
             "7FC£¦–g7G&VÒ–çWB‡&W7VÇEF‚Â7FC£¦–÷3£¦&–æ'’“°¢–b‚–çWB’°¢W'&÷"Ò%—F†öâF–Bæ÷B&öGV6R&W7VÇBf–ÆRâ#°¢&WGW&âfÇ6S°¢Ğ¢7FC£§7G&–ær†VFW#°¢7FC£¦vWFÆ–æR†–çWBÂ†VFW"“°¢òò—F†öâ÷Vç2FW‡Bf–ÆW2v—F‚5$Äböâv–æF÷w3²7FC£¦vWFÆ–æR&VÖ÷fW2ÄböæÇ’à¢–b‚†VFW"æV×G’‚’bb†VFW"æ&6²‚’ÓÒuÇ"r¢†VFW"ç÷ö&6²‚“°¢–b††VFW"ÓÒ$U%$õ""’°¢7FC£¦÷7G&–æw7G&VÒÖW76vS°¢ÖW76vRÃÂ–çWBç&F'Vb‚“°¢W'&÷"ÒÖW76vRç7G"‚“°¢&WGW&âfÇ6S°¢Ğ¢7FC£¦—7G&–æw7G&VÒf–VÆG2††VFW"“°¢7FC£§7G&–ærÖ&¶W#°¢7FC£§6—¦U÷BWFFT6÷VçBÒ°¢7FC£§6—¦U÷B×WFT6÷VçBÒ°¢–b‚†f–VÆG2ãâÖ&¶W"ãâWFFT6÷VçBãâ×WFT6÷VçB’ÇÂÖ&¶W"Ò$ô²"ÇÀ¢WFFT6÷VçBâ‚ÇÂ×WFT6÷VçBâ‚’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BÆ–W"WFFR†VFW#¢"²†VFW#°¢&WGW&âfÇ6S°¢Ğ¢7FC£§6—¦U÷BF—6´6÷VçBÒ°¢f–VÆG2ãâ7FC£§w3°¢–b‚f–VÆG2æVöb‚’bb‚†f–VÆG2ãâF—6´6÷VçB’ÇÂF—6´6÷VçBâb’’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BF—6´”ò6öÖÖæB6÷VçBâ#²&WGW&âfÇ6S°¢Ğ¢GFW&âæF—6´6öÖÖæG2æ6ÆV"‚“°¢GFW&âçÆ–W'2æ6ÆV"‚“°¢GFW&âçÆ–W'2ç&W6W'fR‡WFFT6÷VçB“°¢GFW&âæ×WFW2æ6ÆV"‚“°¢GFW&âæ×WFW2ç&W6W'fR†×WFT6÷VçB“°¢7FC£¦'&“Æ&ööÂÂƒâ6VVåÆ–W'7·Ó°¢f÷"‡7FC£§6—¦U÷BÆ–W$çVÖ&W"Ò²Æ–W$çVÖ&W"ÂWFFT6÷VçC²²·Æ–W$çVÖ&W"’°¢6†"&V6÷&BÒ°¢–çB7F÷VBÒ°¢7FC£§6—¦U÷BWfVçD6÷VçBÒ°¢—F†öåÆ–W%WFFRWFFS°¢–çBÖ÷W6UâÒ°¢–çBÖ÷W6T7WFöfbÒ°¢–b‚†–çWBãâ&V6÷&BãâWFFRçÆ–W$–æFW‚ãâ7F÷VBãâWFFRæ'Òãà¢WFFRæÆVæwF„&VG2ãâWfVçD6÷VçBãâWFFRç7–çF„–æFW‚ãâWFFRæ×Æ—GVFRãà¢WFFRçâãâWFFRæ7WFöfd‡¢ãâWFFRæV6†ô&VG2ãâWFFRæFVÆ•F–ÖU6V6öæG2ãà¢Ö÷W6UâãâÖ÷W6T7WFöfbãâWFFRæGF6µ6V6öæG2ãâWFFRç&VÆV6U6V6öæG2ãà¢WFFRæ×6öçG&öÅFö¶VâãâWFFRçä6öçG&öÅFö¶VâãâWFFRæ7WFöfd6öçG&öÅFö¶Vâãà¢WFFRæV6†ô6öçG&öÅFö¶VâãâWFFRæFVÆ•F–ÖT6öçG&öÅFö¶VâãâWFFRç7–çF…&FRãà¢WFFRç7v–æu6V6öæG2’ÇÀ¢&V6÷&BÒurÇÂWFFRçÆ–W$–æFW‚ÂÇÂWFFRçÆ–W$–æFW‚ãÒ‚ÇÀ¢7F÷VBÂÇÂ7F÷VBâÇÀ¢6VVåÆ–W'5·7FF–5ö67CÇ7FC£§6—¦U÷Câ‡WFFRçÆ–W$–æFW‚•ÒÇÂWfVçD6÷VçBâ#SbÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ'Ò’ÇÂWFFRæ'ÒÂãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæÆVæwF„&VG2’ÇÂWFFRæÆVæwF„&VG2ÃÒãÇÀ¢WFFRæÆVæwF„&VG2âC“bãÇÂWFFRç7–çF„–æFW‚ÂÇÀ¢7FF–5ö67CÇ7FC£§6—¦U÷Câ‡WFFRç7–çF„–æFW‚’ãĞ¢´VÖ&VFFVE7–çF„6÷VçB²W6W%7–çF„6÷VçBÇÀ¢7FC£¦—6f–æ—FR‡WFFRç7–çF…&FR’ÇÂWFFRç7–çF…&FRÂãÇÀ¢WFFRç7–çF…&FRâãÇÀ¢‡WFFRç7–çF„–æFW‚ÓÒRbbWFFRç7–çF…&FRâã’ÇÀ¢7FC£¦—6f–æ—FR‡WFFRç7v–æu6V6öæG2’ÇÀ¢7FC£¦'2‡WFFRç7v–æu6V6öæG2¢WFFRæ'Òòcã’ãÒãC’ÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ×Æ—GVFR’ÇÂWFFRæ×Æ—GVFRÂãÇÂWFFRæ×Æ—GVFRâãÇÀ¢7FC£¦—6f–æ—FR‡WFFRçâ’ÇÂWFFRçâÂÓãÇÂWFFRçââãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ7WFöfd‡¢’ÇÂWFFRæ7WFöfd‡¢Â#ãÇÀ¢WFFRæ7WFöfd‡¢â#ãÇÂ7FC£¦—6f–æ—FR‡WFFRæV6†ô&VG2’ÇÀ¢WFFRæV6†ô&VG2ÂãÇÂWFFRæV6†ô&VG2âcBãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæFVÆ•F–ÖU6V6öæG2’ÇÂWFFRæFVÆ•F–ÖU6V6öæG2ÂãÇÀ¢WFFRæFVÆ•F–ÖU6V6öæG2âBãÇÂÖ÷W6UâÂÇÂÖ÷W6UââÇÀ¢Ö÷W6T7WFöfbÂÇÂÖ÷W6T7WFöfbâÇÀ¢7FC£¦—6f–æ—FR‡WFFRæGF6µ6V6öæG2’ÇÂWFFRæGF6µ6V6öæG2ÂãÇÀ¢WFFRæGF6µ6V6öæG2â2ãÇÂ7FC£¦—6f–æ—FR‡WFFRç&VÆV6U6V6öæG2’ÇÀ¢WFFRç&VÆV6U6V6öæG2ÂãÇÂWFFRç&VÆV6U6V6öæG2â2ã’°¢7FC£¦÷7G&–æw7G&VÒFWF–Ã°¢FWF–ÂÃÂ%—F†öâ&WGW&æVBÖÆf÷&ÖVBÆ–W"WFFRFF‡&V6÷&CÒ"ÃÂ&V6÷&@¢ÃÂ"ÂÆ–W#Ò"ÃÂWFFRçÆ–W$–æFW‚²¢ÃÂ"Â7F÷VCÒ"ÃÂ7F÷VBÃÂ"ÂWfVçG3Ò"ÃÂWfVçD6÷Vç@¢ÃÂ"Â%ÓÒ"ÃÂWFFRæ'ÒÃÂ"Â7–çFƒÒ"ÃÂWFFRç7–çF„–æFW€¢ÃÂ"’â6†V6²F†R6÷–VBF–væ÷7F–2f÷"F†RgVÆÂW†6WF–öââ#°¢W'&÷"ÒFWF–Âç7G"‚“°¢&WGW&âfÇ6S°¢Ğ¢6†"FV×õ&V6÷&BÒ°¢–çBFV×õÆ–W"ÒÓ°¢–b‚†–çWBãâFV×õ&V6÷&BãâFV×õÆ–W"ãâWFFRæ'Ô6öçG&öÅFö¶Vâ’ÇÀ¢FV×õ&V6÷&BÒuBrÇÂFV×õÆ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢WFFRæ'Ô6öçG&öÅFö¶Vâç6—¦R‚’â#Sb’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB6Æö6²æ'ÒWFöÖF–öââ#°¢&WGW&âfÇ6S°¢Ğ¢–b‡WFFRæ'Ô6öçG&öÅFö¶VâÒ"Ò"’°¢6öç7BWFòbFö¶VâÒWFFRæ'Ô6öçG&öÅFö¶Vã°¢6öç7BWFòFVÆ–Ö—FW"ÒFö¶Vâæf–æB‚s¢rÂ"“°¢–b‡Fö¶Vâç6—¦R‚’ÂRÇÂ‡Fö¶Vå³ÒÒwbrbbFö¶Vå³ÒÒvÂr’ÇÀ¢Fö¶Vå³ÒÒs¢rÇÂFVÆ–Ö—FW"ÓÒ7FC£§7G&–æs£¦ç÷2ÇÀ¢FVÆ–Ö—FW"ÓÒ"ÇÂFVÆ–Ö—FW"²ÓÒFö¶Vâç6—¦R‚’ÇÀ¢Fö¶Vâæf–æB‚s¢rÂFVÆ–Ö—FW"²’Ò7FC£§7G&–æs£¦ç÷2’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB6Æö6²æ'ÒWFöÖF–öââ#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢6†"VffV7E&V6÷&BÒ°¢–çBVffV7EÆ–W"ÒÓ°¢–b‚†–çWBãâVffV7E&V6÷&BãâVffV7EÆ–W"ãâWFFRç&WfW&$–æFW‚ãâWFFRç&WfW&$Ö—‚ãà¢WFFRç&WfW&%&ööÒãâWFFRç&WfW&$F×ãâWFFRç&WfW&%F–ÖRãà¢WFFRç&WfW&%7&VB’ÇÀ¢VffV7E&V6÷&BÒu"rÇÂVffV7EÆ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢WFFRç&WfW&$–æFW‚ÂÇÂWFFRç&WfW&$–æFW‚â2ÇÀ¢7FC£¦—6f–æ—FR‡WFFRç&WfW&$Ö—‚’ÇÂWFFRç&WfW&$Ö—‚ÂãÇÂWFFRç&WfW&$Ö—‚âãÇÀ¢7FC£¦—6f–æ—FR‡WFFRç&WfW&%&ööÒ’ÇÂWFFRç&WfW&%&ööÒÂãÇÂWFFRç&WfW&%&ööÒâãÇÀ¢7FC£¦—6f–æ—FR‡WFFRç&WfW&$F×’ÇÂWFFRç&WfW&$F×ÂãÇÂWFFRç&WfW&$F×âãÇÀ¢7FC£¦—6f–æ—FR‡WFFRç&WfW&%F–ÖR’ÇÂWFFRç&WfW&%F–ÖRÂãÇÂWFFRç&WfW&%F–ÖRâãÇÀ¢7FC£¦—6f–æ—FR‡WFFRç&WfW&%7&VB’ÇÂWFFRç&WfW&%7&VBÂãÇÂWFFRç&WfW&%7&VBâã’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB&WfW&"6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6†"FV6•&V6÷&BÒ°¢–çBFV6•Æ–W"ÒÓ°¢–b‚†–çWBãâFV6•&V6÷&BãâFV6•Æ–W"ãâWFFRæV6†ôFV6”&VG2ãâWFFRæV6†ôFV6”6öçG&öÅFö¶Vâãà¢WFFRæV6†ôÖ—‚ãâWFFRæV6†ôÖ—„6öçG&öÅFö¶VâãâWFFRæV6†ôfVVF&6²ãâWFFRæV6†ôfVVF&6´6öçG&öÅFö¶VâãâWFFRææ÷FTFVÆ”6öçG&öÅFö¶Vâ’ÇÀ¢FV6•&V6÷&BÒtRrÇÂFV6•Æ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢7FC£¦—6f–æ—FR‡WFFRæV6†ôFV6”&VG2’ÇÂWFFRæV6†ôFV6”&VG2ÂãÇÂWFFRæV6†ôFV6”&VG2âcBãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæV6†ôÖ—‚’ÇÂWFFRæV6†ôÖ—‚ÂãÇÂWFFRæV6†ôÖ—‚âãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæV6†ôfVVF&6²’ÇÀ¢‡WFFRæV6†ôfVVF&6²ÂãbbWFFRæV6†ôfVVF&6²ÒÓã’ÇÂWFFRæV6†ôfVVF&6²âã“R’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBf÷„F÷BV6†òæBÇVv–âV6†ò6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6†"f–ÇFW%&V6÷&BÒ°¢–çBf–ÇFW%Æ–W"ÒÓ°¢–b‚†–çWBãâf–ÇFW%&V6÷&Bãâf–ÇFW%Æ–W"ãâWFFRæÇd‡¢ãâWFFRæÇ"ãà¢WFFRæ‡d‡¢ãâWFFRæ‡"ãâWFFRæ'd‡¢ãâWFFRæ'"ãà¢WFFRæÇd6öçG&öÅFö¶VâãâWFFRæÇ$6öçG&öÅFö¶Vâãà¢WFFRæ‡d6öçG&öÅFö¶VâãâWFFRæ‡$6öçG&öÅFö¶Vâãà¢WFFRæ'd6öçG&öÅFö¶VâãâWFFRæ'$6öçG&öÅFö¶Vâ’ÇÀ¢f–ÇFW%&V6÷&BÒtbrÇÂf–ÇFW%Æ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢7FC£¦—6f–æ—FR‡WFFRæÇd‡¢’ÇÂWFFRæÇd‡¢ÂãÇÂWFFRæÇd‡¢â#ãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæÇ"’ÇÂWFFRæÇ"ÂãRÇÂWFFRæÇ"âãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ‡d‡¢’ÇÂWFFRæ‡d‡¢ÂãÇÂWFFRæ‡d‡¢â#ãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ‡"’ÇÂWFFRæ‡"ÂãRÇÂWFFRæ‡"âãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ'd‡¢’ÇÂWFFRæ'd‡¢ÂãÇÂWFFRæ'd‡¢â#ãÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ'"’ÇÂWFFRæ'"ÂãRÇÂWFFRæ'"âã’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBf–ÇFW"6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6öç7BWFòfÆ–EÆ–W$VffV7D6öçG&öÅFö¶VâÒµÒ†6öç7B7FC£§7G&–ærbFö¶Vâ’°¢–b‡Fö¶VâÓÒ"Ò"¢&WGW&âG'VS°¢–b‡Fö¶Vâç6—¦R‚’â#SbÇÂFö¶Vâç6—¦R‚’ÂRÇÀ¢‡Fö¶Vå³ÒÒwbrbbFö¶Vå³ÒÒvÂr’ÇÂFö¶Vå³ÒÒs¢r¢&WGW&âfÇ6S°¢6öç7BWFòÖ–FFÆRÒFö¶Vâæf–æB‚s¢rÂ"“°¢–b†Ö–FFÆRÓÒ7FC£§7G&–æs£¦ç÷2ÇÂÖ–FFÆRÓÒ"ÇÀ¢Ö–FFÆR²ãÒFö¶Vâç6—¦R‚’ÇÀ¢Fö¶Vâæf–æB‚s¢rÂÖ–FFÆR²’Ò7FC£§7G&–æs£¦ç÷2¢&WGW&âfÇ6S°¢6öç7BWFò'6TÆ—7BÒµÒ†6öç7B7FC£§7G&–ærbFW‡BÂF÷V&ÆRÖ–æ–×VÒÀ¢F÷V&ÆRÖ†–×VÒÂ&ööÂ÷6—F—fRÀ¢7FC£§fV7F÷#ÆF÷V&ÆSâb÷WGWB’°¢7FC£¦—7G&–æw7G&VÒfÇVW2‡FW‡B“°¢7FC£§7G&–ær'C°¢v†–ÆR‡7FC£¦vWFÆ–æR‡fÇVW2Â'BÂrÂr’’°¢–b‡'BæV×G’‚’ÇÂ÷WGWBç6—¦R‚’ãÒ‚¢&WGW&âfÇ6S°¢G'’°¢7FC£§6—¦U÷BW6VBÒ°¢6öç7BF÷V&ÆRfÇVRÒ7FC£§7FöB‡'BÂgW6VB“°¢–b‡W6VBÒ'Bç6—¦R‚’ÇÂ7FC£¦—6f–æ—FR‡fÇVR’ÇÀ¢‡÷6—F—fRòfÇVRÃÒãÇÂfÇVRâÖ†–×VĞ¢¢fÇVRÂÖ–æ–×VÒÇÂfÇVRâÖ†–×VÒ’¢&WGW&âfÇ6S°¢÷WGWBçW6…ö&6²‡fÇVR“°¢Ò6F6‚‚âââ’°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢&WGW&â÷WGWBæV×G’‚“°¢Ó°¢7FC£§fV7F÷#ÆF÷V&ÆSâfÇVW3°¢7FC£§fV7F÷#ÆF÷V&ÆSâGW&F–öç3°¢&WGW&â'6TÆ—7B‡Fö¶Vâç7V'7G"ƒ"ÂÖ–FFÆRÒ"’ÂãÂBãÂfÇ6RÀ¢fÇVW2’b`¢'6TÆ—7B‡Fö¶Vâç7V'7G"†Ö–FFÆR²’ÂãÂC“bãÂG'VRÀ¢GW&F–öç2’b`¢fÇVW2ç6—¦R‚’ÓÒGW&F–öç2ç6—¦R‚“°¢Ó°¢6†"ÖöGVÆF–öå&V6÷&BÒ°¢–çBÖöGVÆF–öåÆ–W"ÒÓ°¢–b‚†–çWBãâÖöGVÆF–öå&V6÷&BãâÖöGVÆF–öåÆ–W"ãâWFFRæ'æö—6Rãà¢WFFRç7vVÆÂãâWFFRç6†RãâWFFRæG&—fRãà¢WFFRæ'æö—6T6öçG&öÅFö¶VâãâWFFRç7vVÆÄ6öçG&öÅFö¶Vâãà¢WFFRç6†T6öçG&öÅFö¶VâãâWFFRæG&—fT6öçG&öÅFö¶Vâ’ÇÀ¢ÖöGVÆF–öå&V6÷&BÒtrrÇÂÖöGVÆF–öåÆ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢7FC£¦—6f–æ—FR‡WFFRæ'æö—6R’ÇÂWFFRæ'æö—6RÂãÇÀ¢WFFRæ'æö—6RâãÇÂ7FC£¦—6f–æ—FR‡WFFRç7vVÆÂ’ÇÀ¢WFFRç7vVÆÂÂãÇÂWFFRç7vVÆÂâBãÇÀ¢7FC£¦—6f–æ—FR‡WFFRç6†R’ÇÂWFFRç6†RÂãÇÀ¢WFFRç6†RâBãÇÂ7FC£¦—6f–æ—FR‡WFFRæG&—fR’ÇÀ¢WFFRæG&—fRÂãÇÂWFFRæG&—fRâBãÇÀ¢fÆ–EÆ–W$VffV7D6öçG&öÅFö¶Vâ‡WFFRç6†T6öçG&öÅFö¶Vâ’ÇÀ¢fÆ–EÆ–W$VffV7D6öçG&öÅFö¶Vâ‡WFFRæG&—fT6öçG&öÅFö¶Vâ’’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBf–ÇFW"ÖÖöGVÆF–öâ6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6†"7–çF…&V6÷&BÒ°¢–çB7–çF…Æ–W"ÒÓ°¢–b‚†–çWBãâ7–çF…&V6÷&Bãâ7–çF…Æ–W"’ÇÀ¢7–çF…&V6÷&BÒu2rÇÂ7–çF…Æ–W"ÒWFFRçÆ–W$–æFW‚’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB7–çF‚6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6öç7FW‡"7FC£¦'&“ÆF÷V&ÆRÂsâÖ–æ–×V×³ÂÂÂÂãÂÓ#ÂãÓ°¢6öç7FW‡"7FC£¦'&“ÆF÷V&ÆRÂsâÖ†–×V×³ÂÂÂÂ2Â#Â‡Ó°¢f÷"‡7FC£§6—¦U÷B–æFW‚Ò²–æFW‚ÂWFFRç7–çF„6öçG&öÇ2ç6—¦R‚“²²¶–æFW‚’°¢WFòbfÇVRÒWFFRç7–çF„6öçG&öÇ5¶–æFW…Ó°¢–b‚†–çWBãâfÇVR’ÇÂ7FC£¦—6f–æ—FR‡fÇVR’ÇÀ¢fÇVRÂÖ–æ–×VÕ¶–æFW…ÒÇÂfÇVRâÖ†–×VÕ¶–æFW…Ò’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–B7–çF‚6öçG&öÂâ#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢6†"Æ–W$VffV7E&V6÷&BÒ°¢–çBÆ–W$VffV7EÆ–W"ÒÓ°¢–b‚†–çWBãâÆ–W$VffV7E&V6÷&BãâÆ–W$VffV7EÆ–W"ãà¢WFFRçÆ–W$VffV7EW6W$–æFW‚’ÇÀ¢Æ–W$VffV7E&V6÷&BÒtrÇÀ¢Æ–W$VffV7EÆ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢WFFRçÆ–W$VffV7EW6W$–æFW‚ÂÓÇÀ¢‡WFFRçÆ–W$VffV7EW6W$–æFW‚ãÒb`¢7FF–5ö67CÇ7FC£§6—¦U÷Câ‡WFFRçÆ–W$VffV7EW6W$–æFW‚’ãĞ¢W6W%7–çF„6÷VçB’’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BW'6—7FVçBÆ–W"ÖVffV7B6VÆV7F–öââ#°¢&WGW&âfÇ6S°¢Ğ¢6†"VffV7EfÇVW5&V6÷&BÒ°¢–çBVffV7EfÇVW5Æ–W"ÒÓ°¢–b‚†–çWBãâVffV7EfÇVW5&V6÷&BãâVffV7EfÇVW5Æ–W"’ÇÀ¢VffV7EfÇVW5&V6÷&BÒurrÇÀ¢VffV7EfÇVW5Æ–W"ÒWFFRçÆ–W$–æFW‚’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBW'6—7FVçBÖVffV7B6öçG&öÂfÇVW2â#°¢&WGW&âfÇ6S°¢Ğ¢f÷"†WFòbfÇVR¢WFFRçÆ–W$VffV7D6öçG&öÅfÇVW2’°¢–b‚†–çWBãâfÇVR’ÇÂ7FC£¦—6f–æ—FR‡fÇVR’ÇÀ¢fÇVRÂãÇÂfÇVRâã’°¢W'&÷"Ò%—F†öâ&WGW&æVB–çfÆ–Bæ÷&ÖÆ—¦VBÆ–W"ÖVffV7B6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢6†"VffV7D6öçG&öÇ5&V6÷&BÒ°¢–çBVffV7D6öçG&öÇ5Æ–W"ÒÓ°¢–b‚†–çWBãâVffV7D6öçG&öÇ5&V6÷&BãâVffV7D6öçG&öÇ5Æ–W"’ÇÀ¢VffV7D6öçG&öÇ5&V6÷&BÒu‚rÇÀ¢VffV7D6öçG&öÇ5Æ–W"ÒWFFRçÆ–W$–æFW‚’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBW'6—7FVçBÖVffV7B6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6öç7BWFòfÆ–DVffV7D6öçG&öÅFö¶VâÒµÒ†6öç7B7FC£§7G&–ærbFö¶Vâ’°¢–b‡Fö¶VâÓÒ"Ò"¢&WGW&âG'VS°¢–b‡Fö¶Vâç6—¦R‚’â#SbÇÂFö¶Vâç6—¦R‚’ÂRÇÀ¢‡Fö¶Vå³ÒÒwbrbbFö¶Vå³ÒÒvÂr’ÇÂFö¶Vå³ÒÒs¢r¢&WGW&âfÇ6S°¢6öç7BWFòÖ–FFÆRÒFö¶Vâæf–æB‚s¢rÂ"“°¢–b†Ö–FFÆRÓÒ7FC£§7G&–æs£¦ç÷2ÇÂÖ–FFÆRÓÒ"ÇÀ¢Ö–FFÆR²ãÒFö¶Vâç6—¦R‚’ÇÀ¢Fö¶Vâæf–æB‚s¢rÂÖ–FFÆR²’Ò7FC£§7G&–æs£¦ç÷2¢&WGW&âfÇ6S°¢6öç7BWFòfÆ–DçVÖ&W$Æ—7BÒµÒ†6öç7B7FC£§7G&–ærbFW‡BÂF÷V&ÆRÖ–æ–×VÒÀ¢F÷V&ÆRÖ†–×VÒÂ&ööÂ÷6—F—fRÀ¢7FC£§6—¦U÷Bb6÷VçB’°¢–b‡FW‡BæV×G’‚’ÇÂFW‡Bæg&öçB‚’ÓÒrÂrÇÂFW‡Bæ&6²‚’ÓÒrÂrÇÀ¢FW‡Bæf–æB‚"ÂÂ"’Ò7FC£§7G&–æs£¦ç÷2¢&WGW&âfÇ6S°¢7FC£¦—7G&–æw7G&VÒfÇVW2‡FW‡B“°¢7FC£§7G&–ær'C°¢6÷VçBÒ°¢v†–ÆR‡7FC£¦vWFÆ–æR‡fÇVW2Â'BÂrÂr’’°¢–b‡'BæV×G’‚’ÇÂ6÷VçBãÒ‚¢&WGW&âfÇ6S°¢G'’°¢7FC£§6—¦U÷BW6VBÒ°¢6öç7BF÷V&ÆRfÇVRÒ7FC£§7FöB‡'BÂgW6VB“°¢–b‡W6VBÒ'Bç6—¦R‚’ÇÂ7FC£¦—6f–æ—FR‡fÇVR’ÇÀ¢‡÷6—F—fRòfÇVRÃÒãÇÂfÇVRâÖ†–×VĞ¢¢fÇVRÂÖ–æ–×VÒÇÂfÇVRâÖ†–×VÒ’¢&WGW&âfÇ6S°¢Ò6F6‚‚âââ’°¢&WGW&âfÇ6S°¢Ğ¢²¶6÷VçC°¢Ğ¢&WGW&â6÷VçBâ°¢Ó°¢7FC£§6—¦U÷BfÇVT6÷VçBÒ°¢7FC£§6—¦U÷BGW&F–öä6÷VçBÒ°¢&WGW&âfÆ–DçVÖ&W$Æ—7B‡Fö¶Vâç7V'7G"ƒ"ÂÖ–FFÆRÒ"’ÂãÂãÀ¢fÇ6RÂfÇVT6÷VçB’b`¢fÆ–DçVÖ&W$Æ—7B‡Fö¶Vâç7V'7G"†Ö–FFÆR²’ÂãÂC“bãÀ¢G'VRÂGW&F–öä6÷VçB’b`¢†GW&F–öä6÷VçBÓÒÇÂGW&F–öä6÷VçBÓÒfÇVT6÷VçB“°¢Ó°¢f÷"†WFòbFö¶Vâ¢WFFRæVffV7D6öçG&öÅFö¶Vç2’°¢–b‚†–çWBãâFö¶Vâ’ÇÂfÆ–DVffV7D6öçG&öÅFö¶Vâ‡Fö¶Vâ’’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBW'6—7FVçBVffV7BÖ6öçG&öÂÖöGVÆF–öââ#°¢&WGW&âfÇ6S°¢Ğ¢–b‡Fö¶VâÒ"Ò"bbWFFRçÆ–W$VffV7EW6W$–æFW‚Â’°¢W'&÷"Ò%—F†öâ&WGW&æVBW'6—7FVçBÖVffV7BÖöGVÆF–öâv—F†÷WBâVffV7B6VÆV7F–öââ#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢6öç7BWFòfÆ–Dæ÷FTVffV7D6öçG&öÅFö¶VâĞ¢µÒ†6öç7B7FC£§7G&–ærbFö¶VâÂF÷V&ÆRÖ–æ–×VÒÂF÷V&ÆRÖ†–×VÒ’°¢–b‡Fö¶VâÓÒ"Ò"¢&WGW&âG'VS°¢–b‡Fö¶Vâç6—¦R‚’â#SbÇÂFö¶Vâç6—¦R‚’ÂRÇÀ¢‡Fö¶Vå³ÒÒwbrbbFö¶Vå³ÒÒvÂr’ÇÂFö¶Vå³ÒÒs¢r¢&WGW&âfÇ6S°¢6öç7BWFòÖ–FFÆRÒFö¶Vâæf–æB‚s¢rÂ"“°¢–b†Ö–FFÆRÓÒ7FC£§7G&–æs£¦ç÷2ÇÂÖ–FFÆRÓÒ"ÇÀ¢Ö–FFÆR²ãÒFö¶Vâç6—¦R‚’ÇÀ¢Fö¶Vâæf–æB‚s¢rÂÖ–FFÆR²’Ò7FC£§7G&–æs£¦ç÷2¢&WGW&âfÇ6S°¢6öç7BWFò'6TÆ—7BÒµÒ†6öç7B7FC£§7G&–ærbFW‡BÀ¢F÷V&ÆRÆ÷rÂF÷V&ÆR†–v‚À¢&ööÂGW&F–öç2À¢7FC£§6—¦U÷Bb6÷VçB’°¢–b‡FW‡BæV×G’‚’ÇÂFW‡Bæg&öçB‚’ÓÒrÂrÇÀ¢FW‡Bæ&6²‚’ÓÒrÂrÇÂFW‡Bæf–æB‚"ÂÂ"’Ò7FC£§7G&–æs£¦ç÷2¢&WGW&âfÇ6S°¢7FC£¦—7G&–æw7G&VÒfÇVW2‡FW‡B“°¢7FC£§7G&–ær'C°¢6÷VçBÒ°¢v†–ÆR‡7FC£¦vWFÆ–æR‡fÇVW2Â'BÂrÂr’’°¢–b‡'BæV×G’‚’ÇÂ6÷VçBãÒ‚¢&WGW&âfÇ6S°¢G'’°¢7FC£§6—¦U÷BW6VBÒ°¢6öç7BF÷V&ÆRfÇVRÒ7FC£§7FöB‡'BÂgW6VB“°¢–b‡W6VBÒ'Bç6—¦R‚’ÇÂ7FC£¦—6f–æ—FR‡fÇVR’ÇÀ¢†GW&F–öç2òfÇVRÃÒãÇÂfÇVRâC“bã ¢¢fÇVRÂÆ÷rÇÂfÇVRâ†–v‚’¢&WGW&âfÇ6S°¢Ò6F6‚‚âââ’°¢&WGW&âfÇ6S°¢Ğ¢²¶6÷VçC°¢Ğ¢&WGW&â6÷VçBâ°¢Ó°¢7FC£§6—¦U÷BfÇVT6÷VçBÒ°¢7FC£§6—¦U÷BGW&F–öä6÷VçBÒ°¢&WGW&â'6TÆ—7B‡Fö¶Vâç7V'7G"ƒ"ÂÖ–FFÆRÒ"’ÂÖ–æ–×VÒÀ¢Ö†–×VÒÂfÇ6RÂfÇVT6÷VçB’b`¢'6TÆ—7B‡Fö¶Vâç7V'7G"†Ö–FFÆR²’ÂãÂC“bãÀ¢G'VRÂGW&F–öä6÷VçB’b`¢†GW&F–öä6÷VçBÓÒÇÂGW&F–öä6÷VçBÓÒfÇVT6÷VçB“°¢Ó°¢6†"æ÷FTVffV7E&V6÷&BÒ°¢–çBæ÷FTVffV7EÆ–W"ÒÓ°¢–b‚†–çWBãâæ÷FTVffV7E&V6÷&Bãâæ÷FTVffV7EÆ–W"’ÇÀ¢æ÷FTVffV7E&V6÷&BÒu’rÇÂæ÷FTVffV7EÆ–W"ÒWFFRçÆ–W$–æFW‚’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBæ÷FRÖVffV7BF–ÖR×f"6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢6öç7FW‡"7FC£¦'&“ÆF÷V&ÆRÂƒâæ÷FTVffV7DÖ–æ–×V×°¢ãÂÓãÂÓãÂãÂÓãÂãÂãÂãÓ°¢6öç7FW‡"7FC£¦'&“ÆF÷V&ÆRÂƒâæ÷FTVffV7DÖ†–×V×°¢ãÂBãÂBãÂãÂBãÂãÂ3"ãÂCãÓ°¢f÷"‡7FC£§6—¦U÷B–æFW‚Ò°¢–æFW‚ÂWFFRææ÷FTVffV7D6öçG&öÅFö¶Vç2ç6—¦R‚“²²¶–æFW‚’°¢WFòbFö¶VâÒWFFRææ÷FTVffV7D6öçG&öÅFö¶Vç5¶–æFW…Ó°¢–b‚†–çWBãâFö¶Vâ’ÇÀ¢fÆ–Dæ÷FTVffV7D6öçG&öÅFö¶Vâ€¢Fö¶VâÂæ÷FTVffV7DÖ–æ–×VÕ¶–æFW…ÒÂæ÷FTVffV7DÖ†–×VÕ¶–æFW…Ò’’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–Bf–&FWF‚÷6Æ–FRö&VæBö6†÷÷7–âf"6öçG&öÂâ#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢–b‡WFFRç7–çF„–æFW‚ÓÒSr’°¢6†"6öçG&öÅ&V6÷&BÒ°¢–çB6öçG&öÅÆ–W"ÒÓ°¢WFòfÆ–D6öçG&öÅFö¶VâÒµÒ†6öç7B7FC£§7G&–ærbFö¶Vâ’°¢–b‡Fö¶VâÓÒ"Ò"’&WGW&âG'VS°¢–b‡Fö¶Vâç6—¦R‚’â#SbÇÂFö¶Vâç6—¦R‚’ÂRÇÀ¢‡Fö¶Vå³ÒÒwbrbbFö¶Vå³ÒÒvÂr’ÇÂFö¶Vå³ÒÒs¢r¢&WGW&âfÇ6S°¢6öç7BWFòFVÆ–Ö—FW"ÒFö¶Vâæf–æB‚s¢rÂ"“°¢&WGW&âFVÆ–Ö—FW"Ò7FC£§7G&–æs£¦ç÷2bbFVÆ–Ö—FW"â"b`¢FVÆ–Ö—FW"²ÂFö¶Vâç6—¦R‚’b`¢Fö¶Vâæf–æB‚s¢rÂFVÆ–Ö—FW"²’ÓÒ7FC£§7G&–æs£¦ç÷3°¢Ó°¢–b‚†–çWBãâ6öçG&öÅ&V6÷&Bãâ6öçG&öÅÆ–W"ãâWFFRç6×ÆT6öçG&öÅFö¶Vâãà¢WFFRç6×ÆU&FT6öçG&öÅFö¶Vâ’ÇÀ¢6öçG&öÅ&V6÷&BÒurÇÂ6öçG&öÅÆ–W"ÒWFFRçÆ–W$–æFW‚ÇÀ¢fÆ–D6öçG&öÅFö¶Vâ‡WFFRç6×ÆT6öçG&öÅFö¶Vâ’ÇÀ¢fÆ–D6öçG&öÅFö¶Vâ‡WFFRç6×ÆU&FT6öçG&öÅFö¶Vâ’’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB6×ÆRF–ÖR×f"6öçG&öÇ2â#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢6VVåÆ–W'5·7FF–5ö67CÇ7FC£§6—¦U÷Câ‡WFFRçÆ–W$–æFW‚•ÒÒG'VS°¢WFFRç7F÷VBÒ7F÷VBÒ°¢WFFRæÖ÷W6UâÒÖ÷W6UâÒ°¢WFFRæÖ÷W6T7WFöfbÒÖ÷W6T7WFöfbÒ°¢WFFRæWfVçG2ç&W6W'fR†WfVçD6÷VçB“°¢f÷"‡7FC£§6—¦U÷B’Ò²’ÂWfVçD6÷VçC²²¶’’°¢6†"æ÷FU&V6÷&BÒ°¢—F†öäæ÷FTWfVçBWfVçC°¢–b‚†–çWBãâæ÷FU&V6÷&BãâWfVçBæ&VBãâWfVçBæGW&F–öä&VG2ãà¢WfVçBæÖ–F”æ÷FRãâWfVçBçfVÆö6—G’ãâWfVçBæGF6µ6V6öæG2ãà¢WfVçBç&VÆV6U6V6öæG2ãâWfVçBæFVÆ”&VG2ãâWfVçBçf–$FWF‚ãà¢WfVçBç6Æ–FRãâWfVçBç6Æ–FTg&öÒãâWfVçBç6Æ–FTFVÆ’ãà¢WfVçBæ&VæBãâWfVçBæ&VæDFVÆ’ãâWfVçBæ6†÷ãâWfVçBç7–â’ÇÀ¢æ÷FU&V6÷&BÒtârÇÀ¢7FC£¦—6f–æ—FR†WfVçBæ&VB’ÇÂ7FC£¦—6f–æ—FR†WfVçBæGW&F–öä&VG2’ÇÀ¢7FC£¦—6f–æ—FR†WfVçBæFVÆ”&VG2’ÇÂWfVçBæFVÆ”&VG2ÂãÇÂWfVçBæFVÆ”&VG2âRãÇÀ¢7FC£¦—6f–æ—FR†WfVçBçf–$FWF‚’ÇÂWfVçBçf–$FWF‚ÂãÇÀ¢WfVçBçf–$FWF‚âãÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6Æ–FR’ÇÂWfVçBç6Æ–FRÂÓãÇÀ¢WfVçBç6Æ–FRâBãÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6Æ–FTg&öÒ’ÇÂWfVçBç6Æ–FTg&öÒÂÓãÇÀ¢WfVçBç6Æ–FTg&öÒâBãÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6Æ–FTFVÆ’’ÇÂWfVçBç6Æ–FTFVÆ’ÂãÇÀ¢WfVçBç6Æ–FTFVÆ’âãÇÀ¢7FC£¦—6f–æ—FR†WfVçBæ&VæB’ÇÂWfVçBæ&VæBÂÓãÇÀ¢WfVçBæ&VæBâBãÇÀ¢7FC£¦—6f–æ—FR†WfVçBæ&VæDFVÆ’’ÇÂWfVçBæ&VæDFVÆ’ÂãÇÀ¢WfVçBæ&VæDFVÆ’âãÇÀ¢7FC£¦—6f–æ—FR†WfVçBæ6†÷’ÇÂWfVçBæ6†÷ÂãÇÀ¢WfVçBæ6†÷â3"ãÇÀ¢7FC£¦—6f–æ—FR†WfVçBç7–â’ÇÂWfVçBç7–âÂãÇÀ¢WfVçBç7–ââCãÇÀ¢WfVçBæ&VBÂãÇÂWfVçBæ&VBâWFFRæÆVæwF„&VG2ÇÀ¢WfVçBæGW&F–öä&VG2Âã"ÇÀ¢WfVçBæGW&F–öä&VG2â7FC£¦Ö–âƒC“bãÂWFFRæÆVæwF„&VG2¢‚ã’²RÓ’ÇÀ¢WfVçBæÖ–F”æ÷FRÂÇÂWfVçBæÖ–F”æ÷FRâ#rÇÀ¢WfVçBçfVÆö6—G’ÂÇÂWfVçBçfVÆö6—G’â#rÇÀ¢7FC£¦—6f–æ—FR†WfVçBæGF6µ6V6öæG2’ÇÂWfVçBæGF6µ6V6öæG2ÂãÇÀ¢WfVçBæGF6µ6V6öæG2â2ãÇÂ7FC£¦—6f–æ—FR†WfVçBç&VÆV6U6V6öæG2’ÇÀ¢WfVçBç&VÆV6U6V6öæG2ÂãÇÂWfVçBç&VÆV6U6V6öæG2â2ã’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVBæ÷FRWfVçBâ#°¢&WGW&âfÇ6S°¢Ğ¢–b‡WFFRç7–çF„–æFW‚ÓÒSr’°¢6†"'VffW%&V6÷&BÒ°¢–çBvFRÒ°¢–çBÆö÷Ò°¢–b‚†–çWBãâ'VffW%&V6÷&BãâWfVçBç6×ÆU7–Ö&öÂãâWfVçBç6×ÆT–æFW‚ãà¢WfVçBç6×ÆU&FRãâvFRãâWfVçBç6×ÆU7F'BãâWfVçBç6×ÆTVæBãà¢Æö÷’ÇÂ'VffW%&V6÷&BÒt"rÇÀ¢WfVçBç6×ÆU7–Ö&öÂÂ32ÇÂWfVçBç6×ÆU7–Ö&öÂâ#bÇÀ¢WfVçBç6×ÆT–æFW‚ÂÓÇÂWfVçBç6×ÆT–æFW‚âÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6×ÆU&FR’ÇÀ¢7FC£¦'2†WfVçBç6×ÆU&FR’Âãc#RÇÂ7FC£¦'2†WfVçBç6×ÆU&FR’â‚ãÇÀ¢†vFRÒbbvFRÒ’ÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6×ÆU7F'B’ÇÀ¢7FC£¦—6f–æ—FR†WfVçBç6×ÆTVæB’ÇÀ¢WfVçBç6×ÆU7F'BÂãÇÂWfVçBç6×ÆTVæBâãÇÀ¢WfVçBç6×ÆU7F'BãÒWfVçBç6×ÆTVæBÇÀ¢†Æö÷ÒbbÆö÷Ò’’°¢W'&÷"Ò%—F†öâ&WGW&æVBÖÆf÷&ÖVB6×ÆRWfVçBâ#°¢&WGW&âfÇ6S°¢Ğ¢WfVçBç6×ÆTvFRÒvFRÒ°¢WfVçBç6×ÆTÆö÷ÒÆö÷Ò°¢–b†WfVçBç6×ÆTÆö÷bbWfVçBç6×ÆTvFR’°¢W'&÷"Ò%—F†öâ&WGW&æVBÆö÷–ær6×ÆRv—F†÷WB&÷VæFVBvFRâ#°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢WFFRæWfVçG2çW6…ö&6²†WfVçB“°¢Ğ¢7FC£§6÷'B‡WFFRæWfVçG2æ&Vv–â‚’ÂWFFRæWfVçG2æVæB‚’À¢µÒ†6öç7B—F†öäæ÷FTWfVçBbÂ6öç7B—F†öäæ÷FTWfVçBb"’°¢&WGW&âæ&VBÂ"æ&VC°¢Ò“°¢7FC£§fV7F÷#Ç7FC£§—#ÆF÷V&ÆRÂ–çCãâ&÷VæF&–W3°¢&÷VæF&–W2ç&W6W'fR‡WFFRæWfVçG2ç6—¦R‚’¢B“°¢–çB7F—fTæ÷FW2Ò°¢f÷"†6öç7BWFòbWfVçB¢WFFRæWfVçG2’°¢F÷V&ÆR7F'D&VBÒ7FC£¦fÖöB†WfVçBæ&VB²WfVçBæFVÆ”&VG2ÂWFFRæÆVæwF„&VG2“°¢–b‡7F'D&VBÂã’7F'D&VB³ÒWFFRæÆVæwF„&VG3°¢6öç7BF÷V&ÆRgVÆÄ7–6ÆW2Ò7FC£¦fÆö÷"†WfVçBæGW&F–öä&VG2òWFFRæÆVæwF„&VG2“°¢–b†gVÆÄ7–6ÆW2â‚ãÇÂ7F—fTæ÷FW2²7FF–5ö67CÆ–çCâ†gVÆÄ7–6ÆW2’â‚’°¢7FC£¦÷7G&–æw7G&VÒÖW76vS°¢ÖW76vRÃÂ%Æ–W""ÃÂ‡WFFRçÆ–W$–æFW‚²¢ÃÂ"W†6VVG2‚Öæ÷FRöÇ—†öç’7&÷72÷fW&Æ–ær7W7F–ç2â#°¢W'&÷"ÒÖW76vRç7G"‚“°¢&WGW&âfÇ6S°¢Ğ¢7F—fTæ÷FW2³Ò7FF–5ö67CÆ–çCâ†gVÆÄ7–6ÆW2“°¢6öç7BF÷V&ÆR&VÖ–æFW"Ğ¢WfVçBæGW&F–öä&VG2ÒgVÆÄ7–6ÆW2¢WFFRæÆVæwF„&VG3°¢–b‡&VÖ–æFW"ÃÒRÓ’¢6öçF–çVS°¢6öç7BF÷V&ÆRVæD&VBÒ7F'D&VB²&VÖ–æFW#°¢–b†VæD&VBÃÒWFFRæÆVæwF„&VG2’°¢&÷VæF&–W2æV×Æ6Uö&6²‡7F'D&VBÂ“°¢&÷VæF&–W2æV×Æ6Uö&6²†VæD&VBÂÓ“°¢ÒVÇ6R°¢&÷VæF&–W2æV×Æ6Uö&6²ƒãÂ“°¢&÷VæF&–W2æV×Æ6Uö&6²†VæD&VBÒWFFRæÆVæwF„&VG2ÂÓ“°¢&÷VæF&–W2æV×Æ6Uö&6²‡7F'D&VBÂ“°¢&÷VæF&–W2æV×Æ6Uö&6²‡WFFRæÆVæwF„&VG2ÂÓ“°¢Ğ¢Ğ¢7FC£§6÷'B†&÷VæF&–W2æ&Vv–â‚’Â&÷VæF&–W2æVæB‚’À¢µÒ†6öç7BWFòbÂ6öç7BWFòb"’°¢–b†æf—'7BÒ"æf—'7B’&WGW&âæf—'7BÂ"æf—'7C°¢&WGW&âç6V6öæBÂ"ç6V6öæC°¢Ò“°¢f÷"†6öç7BWFòb&÷VæF'’¢&÷VæF&–W2’°¢7F—fTæ÷FW2³Ò&÷VæF'’ç6V6öæC°¢–b†7F—fTæ÷FW2â‚’°¢7FC£¦÷7G&–æw7G&VÒÖW76vS°¢ÖW76vRÃÂ%Æ–W""ÃÂ‡WFFRçÆ–W$–æFW‚²¢ÃÂ"W†6VVG2‚Öæ÷FRöÇ—†öç’æV"&VB"ÃÂ&÷VæF'’æf—'7BÃÂrâs°¢W'&÷"ÒÖW76vRç7G"‚“°¢&WGW&âfÇ6S°¢Ğ¢Ğ¢GFW&âçÆ–W'2çW6…ö&6²‡7FC£¦Ö÷fR‡WFFR’“°¢Ğ¢7FC£¦'&“Æ&ööÂÂƒâ6VVä×WFW7·Ó°¢f÷"‡7FC£§6—¦U÷B’Ò²’Â×WFT6÷VçC²²¶’’°¢6†"&V6÷&BÒ°¢–çBÆ–W$–æFW‚ÒÓ°¢–çB×WFVBÒÓ°¢–b‚†–çWBãâ&V6÷&BãâÆ–W$–æFW‚ãâ×WFVB’ÇÂ&V6÷&BÒtÒrÇÀ¢Æ–W$–æFW‚ÂÇÂÆ–W$–æFW‚ãÒ‚ÇÂ×WFVBÂÇÂ×WFVBâÇÀ¢6VVä×WFW5·7FF–5ö67CÇ7FC£§6—¦U÷Câ‡Æ–W$–æFW‚•Ò’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BÆ–W"×WFR6öçG&öÂ‡&V6÷&BÂÆ–W"–æFW‚Â÷"×WFR7FFR’â#°¢&WGW&âfÇ6S°¢Ğ¢6VVä×WFW5·7FF–5ö67CÇ7FC£§6—¦U÷Câ‡Æ–W$–æFW‚•ÒÒG'VS°¢GFW&âæ×WFW2çW6…ö&6²…—F†öåÆ–W$×WFW·Æ–W$–æFW‚Â×WFVBÒÒ“°¢Ğ¢f÷"‡7FC£§6—¦U÷B’Ò²’ÂF—6´6÷VçC²²¶’’°¢—F†öäF—6´6öÖÖæB6öÖÖæC°¢6†"&V6÷&BÒ°¢–çBÆö÷Ò°¢7FC£§7G&–ærF„†Wƒ°¢–b‚†–çWBãâ&V6÷&Bãâ6öÖÖæBæÖöFRãâ6öÖÖæBç6Æ÷Bãâ6öÖÖæBç&FRãà¢6öÖÖæBæ×ãâ6öÖÖæBçâãâ6öÖÖæBç6V6öæG2ãâÆö÷ãâF„†W‚’ÇÀ¢&V6÷&BÒtBrÇÂ6öÖÖæBæÖöFRÂÇÂ6öÖÖæBæÖöFRâBÇÀ¢6öÖÖæBç6Æ÷BÂÇÂ6öÖÖæBç6Æ÷BârÇÂ†Æö÷ÒbbÆö÷Ò’ÇÀ¢7FC£¦—6f–æ—FR†6öÖÖæBç&FR’ÇÂ6öÖÖæBç&FRÂãc#RÇÂ6öÖÖæBç&FRâ‚ÇÀ¢7FC£¦—6f–æ—FR†6öÖÖæBæ×’ÇÂ6öÖÖæBæ×ÂÇÂ6öÖÖæBæ×â"ÇÀ¢7FC£¦—6f–æ—FR†6öÖÖæBçâ’ÇÂ6öÖÖæBçâÂÓÇÂ6öÖÖæBçââÇÀ¢7FC£¦—6f–æ—FR†6öÖÖæBç6V6öæG2’ÇÀ¢†6öÖÖæBæÖöFRÓÒ2bb†6öÖÖæBç6V6öæG2ÂãRÇÂ6öÖÖæBç6V6öæG2â3c’’ÇÀ¢†6öÖÖæBæÖöFRÒ2bb6öÖÖæBç6V6öæG2Ò’’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BF—6´”ò6öÖÖæBâ#²&WGW&âfÇ6S°¢Ğ¢6öÖÖæBæÆö÷ÒÆö÷Ò°¢–b†6öÖÖæBæÖöFRÓÒÇÂ6öÖÖæBæÖöFRÓÒ"’°¢–b‡F„†W‚æV×G’‚’ÇÂF„†W‚ç6—¦R‚’âC“bÇÂF„†W‚ç6—¦R‚’R"’°¢W'&÷"Ò%—F†öâ&WGW&æVBâ–çfÆ–BF—6²–çWBF‚â#²&WGW&âfÇ6S°¢Ğ¢WFòF–v—BÒµÒ†6†"2’°¢&WGW&â2ãÒsrbb2ÃÒs’rò2Òsr ¢2ãÒvrbb2ÃÒvbrò2Òvr²¢Ó°¢Ó°¢f÷"‡7FC£§6—¦U÷B÷2Ò²÷2ÂF„†W‚ç6—¦R‚“²÷2³Ò"’°¢6öç7BWFò†–v‚ÒF–v—B‡F„†W…·÷5Ò’ÂÆ÷rÒF–v—B‡F„†W…·÷2²Ò“°¢–b††–v‚ÂÇÂÆ÷rÂÇÂ†–v‚¢b²Æ÷rÂ3"’°¢W'&÷"Ò%—F†öâ&WGW&æVB–çfÆ–BF—6²F‚Væ6öF–ærâ#²&WGW&âfÇ6S°¢Ğ¢6öÖÖæBçF‚çW6…ö&6²‡7FF–5ö67CÆ6†#â††–v‚¢b²Æ÷r’“°¢Ğ¢ÒVÇ6R–b‡F„†W‚Ò"Ò"’°¢W'&÷"Ò%—F†öâ&WGW&æVBF‚f÷"æöâ×&VF–ærF—6²6öÖÖæBâ#²&WGW&âfÇ6S°¢Ğ¢GFW&âæF—6´6öÖÖæG2çW6…ö&6²‡7FC£¦Ö÷fR†6öÖÖæB’“°¢Ğ¢&WGW&âG'VS°§Ğ§ÒòòæÖW76P ¥÷'F&ÆU—F†öâb÷'F&ÆU—F†öã£¦–ç7Fæ6R‚’°¢7FF–2÷'F&ÆU—F†öâ'VçF–ÖS°¢&WGW&â'VçF–ÖS°§Ğ ¦&ööÂ÷'F&ÆU—F†öã£¦–æ—F–Æ—¦R†6öç7B7FC£¦f–ÆW7—7FVÓ£§F‚b'VçF–ÖTF—&V7F÷'’À¢7FC£§7G&–ærbW'&÷"’°¢Æ–fV7–6ÆUG&6RG&6R‚%—F†öâ–æ—F–Æ—¦R–æ6ÇVF–ærÆö6²"ÂF†—2“°¢6–bFVf–æVB…õt”ã3"¢7FC£¦Æö6µöwV&CÇ7FC£¦×WFWƒâÆö6²†u—F†öä×WFW‚“°¢–b†–æ—F–Æ—¦VEò¢&WGW&âG'VS° ¢6öç7BWFòFÆÅF‚Ò'VçF–ÖTF—&V7F÷'’òÂ'—F†öã3"æFÆÂ#°¢u—F†öâæÖöGVÆRÒÆöDÆ–'&'”W…r†FÆÅF‚æ5÷7G"‚’ÂçVÆÇG"ÂÄôEõt•D…ôÅDU$TEõ4T$4…õD‚“°¢–b‚u—F†öâæÖöGVÆR’°¢W'&÷"Ò$6÷VÆBæ÷BÆöB'VæFÆVB—F†öâ2ã"‡—F†öã3"æFÆÂ’â#°¢&WGW&âfÇ6S°¢Ğ ¢–b‚ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%•õ6WE—F†öä†öÖR"Âu—F†öâç6WE—F†öä†öÖR’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%•ô–æ—F–Æ—¦TW‚"Âu—F†öâæ–æ—F–Æ—¦TW‚’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%•ô—4–æ—F–Æ—¦VB"Âu—F†öâæ—4–æ—F–Æ—¦VB’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%”WfÅõ6fUF‡&VB"Âu—F†öâæWfÅ6fUF‡&VB’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%•'Våõ6–×ÆU7G&–ætfÆw2"Âu—F†öâç'Vå6–×ÆU7G&–ætfÆw2’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%”t”Å7FFUôVç7W&R"Âu—F†öâæv–Å7FFTVç7W&R’ÇÀ¢ÆöDgVæ7F–öâ†u—F†öâæÖöGVÆRÂ%”t”Å7FFUõ&VÆV6R"Âu—F†öâæv–Å7FFU&VÆV6R’’°¢W'&÷"Ò%F†R'VæFÆVB—F†öâ'VçF–ÖR—2Ö—76–ær&WV—&VBVÖ&VFF–ærgVæ7F–öç2â#°¢&WGW&âfÇ6S°¢Ğ ¢7FF–27FC£§w7G&–ær—F†öä†öÖS°¢—F†öä†öÖRÒ'VçF–ÖTF—&V7F÷'’çw7G&–ær‚“°¢u—F†öâç6WE—F†öä†öÖR‡—F†öä†öÖRæ5÷7G"‚’“°¢u—F†öâæ–æ—F–Æ—¦TW‚ƒ“°¢–b‚u—F†öâæ—4–æ—F–Æ—¦VB‚’’°¢W'&÷"Ò%—F†öâf–ÆVBFò–æ—F–Æ—¦Rg&öÒF†R'VæFÆVB'VçF–ÖRâ#°¢&WGW&âfÇ6S°¢Ğ ¢òòF†R–çFW'&WFW"×W7Bæ÷B&WF–âF†Rt”ÂöâF†RF‡&VBF†B–æ—F–Æ—¦V@¢òò—C²VF—F÷"×G&–vvW&VBWfÇVF–öç2Ö’6öÖRg&öÒF–ffW&VçB†÷7BF‡&VBà¢u—F†öâæWfÅ6fUF‡&VB‚“°¢–æ—F–Æ—¦VEòÒG'VS°¢&WGW&âG'VS°¢6VÇ6P¢‡fö–B—'VçF–ÖTF—&V7F÷'“°¢W'&÷"Ò%F†R÷'F&ÆRVÖ&VFFVB—F†öâ'VçF–ÖR—27W'&VçFÇ’6¶vVBf÷"v–æF÷w2â#°¢&WGW&âfÇ6S°¢6VæF–`§Ğ ¦&ööÂ÷'F&ÆU—F†öã£¦WfÇVFR†6öç7B7FC£¦f–ÆW7—7FVÓ£§F‚b'VçF–ÖTF—&V7F÷'’À¢6öç7B7FC£§7G&–ærbæÖW76TæÖRÀ¢6öç7B7FC£§7G&–ærb6÷W&6RÀ¢6öç7B7FC£§fV7F÷#Ç7FC£§7G&–æsâbW6W%7–çF„æÖW2À¢—F†öåGFW&âbGFW&âÀ¢7FC£§7G&–ærbW'&÷"’°¢–b‡6÷W&6Rç6—¦R‚’âcB¢#B’°¢W'&÷"Ò$6öFR—2Æ–Ö—FVBFòcB´"–âF†—2&÷F÷G—Râ#°¢&WGW&âfÇ6S°¢Ğ¢–b‚–æ—F–Æ—¦R‡'VçF–ÖTF—&V7F÷'’ÂW'&÷"’¢&WGW&âfÇ6S° ¢6–bFVf–æVB…õt”ã3"¢6öç7BWFòçVÖ&W"Òu67&—DçVÖ&W"æfWF6…öFBƒÂ7FC£¦ÖVÖ÷'•ö÷&FW%÷&VÆ†VB“°¢6öç7BWFò&6RÒ7FC£¦f–ÆW7—7FVÓ£§FV×öF—&V7F÷'•÷F‚‚’ğ¢„Â&FW‡FW"×—F†öâÒ"²7FC£§Fõ÷w7G&–ær„vWD7W'&VçE&ö6W74–B‚’’²Â"Ò"°¢7FC£§Fõ÷w7G&–ær†çVÖ&W"’“°¢6öç7BWFò6÷W&6UF‚Ò&6Rçw7G&–ær‚’²Â"ç’#°¢6öç7BWFò&W7VÇEF‚Ò&6Rçw7G&–ær‚’²Â"æ÷WB#° ¢°¢7FC£¦ög7G&VÒ6÷W&6Tf–ÆR‡6÷W&6UF‚Â7FC£¦–÷3£¦&–æ'’“°¢–b‚6÷W&6Tf–ÆR’°¢W'&÷"Ò$6÷VÆBæ÷B7&VFRFV×÷&'’—F†öâ6÷W&6Rf–ÆRâ#°¢&WGW&âfÇ6S°¢Ğ¢6÷W&6Tf–ÆRçw&—FR‡6÷W&6RæFF‚’Â7FF–5ö67CÇ7FC£§7G&V×6—¦Sâ‡6÷W&6Rç6—¦R‚’’“°¢Ğ ¢6öç7BWFò6÷W&6UWFc‚ÒF„5WFc‚‡6÷W&6UF‚“°¢6öç7BWFò&W7VÇEWFc‚ÒF„5WFc‚‡&W7VÇEF‚“°¢6öç7BWFò67&—BÒÖ¶TWfÇVF–öå67&—B‡6÷W&6UWFc‚Â&W7VÇEWFc‚ÂæÖW76TæÖRÀ¢W6W%7–çF„æÖW2“°¢–çB&W7VÇBÒÓ°¢°¢7FC£¦Æö6µöwV&CÇ7FC£¦×WFWƒâÆö6²†u—F†öä×WFW‚“°¢6öç7B–çBv–ÂÒu—F†öâæv–Å7FFTVç7W&R‚“°¢&W7VÇBÒu—F†öâç'Vå6–×ÆU7G&–ætfÆw2‡67&—Bæ5÷7G"‚’ÂçVÆÇG"“°¢u—F†öâæv–Å7FFU&VÆV6R†v–Â“°¢Ğ ¢&ööÂ7V66W72ÒfÇ6S°¢–b‡&W7VÇBÓÒ¢7V66W72Ò'6U&W7VÇB‡&W7VÇEF‚ÂGFW&âÂW6W%7–çF„æÖW2ç6—¦R‚’ÂW'&÷"“°¢VÇ6P¢W'&÷"Ò%—F†öâ6÷VÆBæ÷BWfÇVFRF†RVF—F÷"67&—Bâ#° ¢7FC£¦W'&÷%ö6öFR–væ÷&VC°¢7FC£¦f–ÆW7—7FVÓ£§&VÖ÷fR‡6÷W&6UF‚Â–væ÷&VB“°¢7FC£¦f–ÆW7—7FVÓ£§&VÖ÷fR‡&W7VÇEF‚Â–væ÷&VB“°¢&WGW&â7V66W73°¢6VÇ6P¢‡fö–B–æÖW76TæÖS°¢‡fö–B—6÷W&6S°¢‡fö–B—GFW&ã°¢&WGW&âfÇ6S°¢6VæF–`§Ğ ¦&ööÂ÷'F&ÆU—F†öã£¦6†V6´6ö×F–&–Æ—G’†6öç7B7FC£¦f–ÆW7—7FVÓ£§F‚b'VçF–ÖTF—&V7F÷'’À¢6öç7B7FC£§7G&–ærb6÷W&6RÀ¢6öç7B7FC£§fV7F÷#Ç7FC£§7G&–æsâbW6W%7–çF„æÖW2À¢7FC£§7G&–ærb&W÷'BÀ¢7FC£§7G&–ærbW'&÷"’°¢–b‡6÷W&6Rç6—¦R‚’âcB¢#B’°¢W'&÷"Ò$6öFR—2Æ–Ö—FVBFòcB´"–âF†—2&÷F÷G—Râ#°¢&WGW&âfÇ6S°¢Ğ¢–b‚–æ—F–Æ—¦R‡'VçF–ÖTF—&V7F÷'’ÂW'&÷"’¢&WGW&âfÇ6S° ¢6–bFVf–æVB…õt”ã3"¢6öç7BWFòçVÖ&W"Òu67&—DçVÖ&W"æfWF6…öFBƒÂ7FC£¦ÖVÖ÷'•ö÷&FW%÷&VÆ†VB“°¢6öç7BWFò&6RÒ7FC£¦f–ÆW7—7FVÓ£§FV×öF—&V7F÷'•÷F‚‚’ğ¢„Â&FW‡FW"Ö6†V6²Ò"²7FC£§Fõ÷w7G&–ær„vWD7W'&VçE&ö6W74–B‚’’²Â"Ò"°¢7FC£§Fõ÷w7G&–ær†çVÖ&W"’“°¢6öç7BWFò6÷W&6UF‚Ò&6Rçw7G&–ær‚’²Â"ç’#°¢6öç7BWFò&W÷'EF‚Ò&6Rçw7G&–ær‚’²Â"çG‡B#°¢°¢7FC£¦ög7G&VÒ6÷W&6Tf–ÆR‡6÷W&6UF‚Â7FC£¦–÷3£¦&–æ'’“°¢–b‚6÷W&6Tf–ÆR’°¢W'&÷"Ò$6÷VÆBæ÷B7&VFRFV×÷&'’—F†öâ6÷W&6Rf–ÆRf÷"6ö×F–&–Æ—G’6†V6¶–ærâ#°¢&WGW&âfÇ6S°¢Ğ¢6÷W&6Tf–ÆRçw&—FR‡6÷W&6RæFF‚’Â7FF–5ö67CÇ7FC£§7G&V×6—¦Sâ‡6÷W&6Rç6—¦R‚’’“°¢Ğ ¢6öç7BWFò67&—BÒÖ¶T6ö×F–&–Æ—G•67&—B‡F„5WFc‚‡6÷W&6UF‚’À¢F„5WFc‚‡&W÷'EF‚’À¢W6W%7–çF„æÖW2“°¢–çB&W7VÇBÒÓ°¢°¢7FC£¦Æö6µöwV&CÇ7FC£¦×WFWƒâÆö6²†u—F†öä×WFW‚“°¢6öç7B–çBv–ÂÒu—F†öâæv–Å7FFTVç7W&R‚“°¢&W7VÇBÒu—F†öâç'Vå6–×ÆU7G&–ætfÆw2‡67&—Bæ5÷7G"‚’ÂçVÆÇG"“°¢u—F†öâæv–Å7FFU&VÆV6R†v–Â“°¢Ğ ¢&ööÂ7V66W72ÒfÇ6S°¢–b‡&W7VÇBÓÒ’°¢7FC£¦–g7G&VÒ&W÷'Df–ÆR‡&W÷'EF‚Â7FC£¦–÷3£¦&–æ'’“°¢–b‡&W÷'Df–ÆR’°¢7FC£¦÷7G&–æw7G&VÒ÷WGWC°¢÷WGWBÃÂ&W÷'Df–ÆRç&F'Vb‚“°¢&W÷'BÒ÷WGWBç7G"‚“°¢7V66W72Ò&W÷'BæV×G’‚“°¢Ğ¢–b‚7V66W72¢W'&÷"Ò%F†R6ö×F–&–Æ—G’6†V6¶W"F–Bæ÷B&öGV6R&W÷'Bâ#°¢ÒVÇ6R°¢W'&÷"Ò%F†RVÖ&VFFVB—F†öâ6ö×F–&–Æ—G’6†V6¶W"6÷VÆBæ÷B'Vââ#°¢Ğ ¢7FC£¦W'&÷%ö6öFR–væ÷&VC°¢7FC£¦f–ÆW7—7FVÓ£§&VÖ÷fR‡6÷W&6UF‚Â–væ÷&VB“°¢7FC£¦f–ÆW7—7FVÓ£§&VÖ÷fR‡&W÷'EF‚Â–væ÷&VB“°¢&WGW&â7V66W73°¢6VÇ6P¢‡fö–B—6÷W&6S°¢‡fö–B—&W÷'C°¢W'&÷"Ò%F†R÷'F&ÆRVÖ&VFFVB—F†öâ'VçF–ÖR—27W'&VçFÇ’6¶vVBf÷"v–æF÷w2â#°¢&WGW&âfÇ6S°¢6VæF–`§Ğ