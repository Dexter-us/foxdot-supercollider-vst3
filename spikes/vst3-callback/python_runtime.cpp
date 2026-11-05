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
    'P10', 'PBern', 'PSine', 'PTri', 'PSum', 'PIndex', 'PxRand', 'PwRand',
    'PZ12', 'PFibMod',
    'PSquare', 'PSq', 'PRange', 'PStep', 'PEuclid', 'PEuclid2',
    'play', 'sequence', 'saw', 'pulse', 'sine', 'triangle', 'bass', 'varsaw',
    'dirt', 'jbass', 'noise', 'dub', 'growl', 'soprano', 'crunch', 'lazer', 'rave',
    'bell', 'gong', 'marimba', 'pluck', 'twang', 'donk', 'squish',
    'charm', 'spark', 'blip', 'scatter', 'orient', 'zap', 'fuzz', 'bug',
    'ripple', 'viola', 'nylon', 'arpy', 'dab', 'sawbass', 'dbass',
    'prophet', 'pads', 'pasha', 'ambi', 'sinepad', 'keys', 'klank',
    'feel', 'glass', 'soft', 'quin', 'space', 'scratch', 'creep',
    'snick', 'swell', 'razz', 'sitar', 'star', 'karp',
    'var', 'linvar', 'Clock', 'Seed', 'Scale', 'Root', 'PGroup',
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
    'stutter', 'palindrome', 'count', 'index', 'submap', 'concat',
    'add', 'abs', 'all', 'startswith', 'limit', 'every', 'copy',
    'asGroup', 'group', 'flatten', 'int', 'float', 'str', 'append', 'extend',
    'insert', 'pop', 'remove', 'set', 'alt', 'string', 'items', 'getitem',
    'getslice', 'setitem', 'eq', 'ne', 'i_rotate', 'i_reverse', 'i_sort',
    'i_shuf', 'transform', 'true_copy', 'convert_data', 'get_data',
    'get_methods', 'help', 'new', 'make', 'get_behaviour'
}
_COMPAT_PATTERN_CALLS = {
    'PAlt', 'PStretch', 'PStutter', 'PShuf', 'PJoin', 'PBeat', 'PDur',
    'PDelay', 'PStrum', 'PQuicken', 'PRhythm', 'PZip', 'PZip2', 'PRand',
    'PWhite', 'PWalk', 'PChain', 'PDelta', 'Pvar', 'PZ12', 'PFibMod',
    'PSquare', 'PSq',
    'PRange', 'PStep', 'PEuclid', 'PEuclid2', 'P10', 'PBern', 'PSine',
    'PTri', 'PSum', 'PIndex', 'PxRand', 'PwRand', 'reverse', 'rotate', 'repeat',
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
    'bend', 'benddelay', 'pshift', 'bits', 'crush', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur',
    'shape', 'drive', 'chop', 'spin', 'cut', 'coarse', 'formant', 'effect', 'fx_vib', 'fx_depth',
    'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60',
    'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq',
    'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone',
    'fx_center', 'fx_shape', 'fx_phase', 'fx_time', 'fx_period',
    'fx_length', 'fx_attack'
}
_USER_SYNTH_KEYWORDS = {
    'notes', 'dur', 'oct', 'sus', 'scale', 'root', 'amp', 'pan', 'cutoff',
    'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'velocity',
    'rate', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime',
    'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell',
    'strum', 'vib', 'depth', 'vibdepth', 'slide', 'slidefrom', 'slidedelay',
    'bend', 'benddelay', 'pshift', 'bits', 'crush', 'lforate', 'lfowidth', 'rq', 'fmod', 'blur',
    'shape', 'drive', 'chop', 'spin', 'cut', 'coarse', 'formant', 'effect', 'fx_vib', 'fx_depth',
    'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60',
    'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq',
    'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone',
    'fx_center', 'fx_shape', 'fx_phase', 'fx_time', 'fx_period',
    'fx_length', 'fx_attack'
}
_COMPAT_SEQUENCE_KEYWORDS = {
    'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'synth',
    'amp', 'pan', 'cutoff', 'echo', 'decay', 'delay', 'delaytime', 'echomix', 'feedback', 'atk', 'rel', 'reverb', 'room', 'mix', 'damp', 'revtime', 'spread', 'lpf', 'lpr', 'hpf', 'hpr', 'bpf', 'bpr', 'bpnoise', 'swell', 'root', 'strum', 'rate',
    'shape', 'drive', 'vib', 'depth', 'vibdepth', 'slide', 'slidefrom',
    'slidedelay', 'bend', 'benddelay', 'pshift', 'bits', 'crush', 'chop', 'spin', 'cut', 'coarse', 'formant', 'effect',
    'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq',
    'fx_blur', 'fx_mix', 'fx_t60', 'fx_damp', 'fx_size',
    'fx_earlydiff', 'fx_moddepth', 'fx_modfreq', 'fx_delay', 'fx_diff',
    'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone', 'fx_center',
    'fx_shape', 'fx_phase', 'fx_time', 'fx_period', 'fx_length', 'fx_attack'
}
_COMPAT_NOTE_PLAY_KEYWORDS = {'notes', 'step', 'duration', 'velocity', 'offset', 'degrees', 'strum', 'pshift', 'bits', 'crush', 'shape', 'drive', 'chop', 'spin', 'cut', 'coarse', 'formant', 'effect', 'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur', 'fx_mix', 'fx_t60', 'fx_damp', 'fx_size', 'fx_earlydiff', 'fx_moddepth', 'fx_modfreq', 'fx_delay', 'fx_diff', 'fx_feedback', 'fx_rate', 'fx_spread', 'fx_tone', 'fx_center', 'fx_shape', 'fx_phase', 'fx_time', 'fx_period', 'fx_length', 'fx_attack'}
_COMPAT_PLAYER_EFFECT_CONTROLS = {
    'AirwindowsTapeDelay2': {
        'fx_vib', 'fx_depth', 'fx_lforate', 'fx_lfowidth', 'fx_rq', 'fx_blur'},
    'AirwindowsEchojam': {
        'fx_vib', 'fx_depth', 'fx_lforate', 'fx_blur'},
    'JPverb': {
        'fx_mix', 'fx_t60', 'fx_damp', 'fx_size', 'fx_earlydiff',
        'fx_moddepth', 'fx_modfreq'},
    'Greyhole': {
        'fx_mix', 'fx_delay', 'fx_damp', 'fx_size', 'fx_diff',
        'fx_feedback', 'fx_moddepth'},
    'Chorus': {
        'fx_mix', 'fx_rate', 'fx_depth', 'fx_delay', 'fx_spread',
        'fx_feedback', 'fx_tone'},
    'Flanger': {
        'fx_mix', 'fx_rate', 'fx_depth', 'fx_delay', 'fx_spread',
        'fx_feedback', 'fx_tone'},
    'Phaser': {
        'fx_mix', 'fx_rate', 'fx_depth', 'fx_center', 'fx_spread',
        'fx_feedback', 'fx_tone'},
    'Tremolo': {
        'fx_mix', 'fx_rate', 'fx_depth', 'fx_shape', 'fx_spread',
        'fx_phase', 'fx_tone'},
    'LimutPingPong': {'fx_mix', 'fx_time', 'fx_feedback'},
    'LimutDuck': {
        'fx_mix', 'fx_period', 'fx_depth', 'fx_length', 'fx_attack'}
}
for _effect_keywords in (_COMPAT_WAVE_KEYWORDS, _USER_SYNTH_KEYWORDS,
                         _COMPAT_SEQUENCE_KEYWORDS, _COMPAT_NOTE_PLAY_KEYWORDS):
    _effect_keywords.add('effects')
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
    'chop', 'spin', 'cut', 'coarse', 'formant'
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
        self.pvar_names = set()
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

    def _is_pvar_expression(self, node):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            return node.func.id == 'Pvar'
        return isinstance(node, ast.Name) and node.id in self.pvar_names

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
            if node.func.id == 'PIndex':
                return 'numeric'
            if node.func.id in {'PxRand', 'PwRand'}:
                sourceName = 'start' if node.func.id == 'PxRand' else 'values'
                source = (node.args[0] if node.args else next(
                    (keyword.value for keyword in node.keywords
                     if keyword.arg == sourceName), None))
                if source is not None:
                    return self._pattern_kind(source)
            if node.func.id in {'PRange', 'PSq', 'PEuclid', 'P10', 'PBern',
                                'PSine', 'PTri', 'PSum'}:
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
                node.func.attr not in _COMPAT_PATTERN_METHODS and
                not (node.func.attr == 'expand' and
                     self._is_pvar_expression(node.func.value))):
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
        if (isinstance(node.func, ast.Attribute) and
                isinstance(node.func.value, ast.Name) and
                node.func.value.id == 'Clock' and node.func.attr == 'clear'):
            if node.args or node.keywords:
                self.add(node.lineno, 'Clock.clear accepts no arguments.')
        clock_clear_call = (
            isinstance(node.func, ast.Attribute) and
            isinstance(node.func.value, ast.Name) and
            node.func.value.id == 'Clock' and node.func.attr == 'clear')
        if (name and name not in _COMPAT_CALLS and name not in self.defined and
                not clock_clear_call):
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
        if name == 'P10':
            self._check_fixed_signature(node, name, ('n', 'seed'), 1)
        if name == 'PBern':
            self._check_fixed_signature(
                node, name, ('size', 'ratio', 'seed'), 0)
        if name == 'PSine':
            self._check_fixed_signature(node, name, ('n',), 0)
        if name == 'PTri':
            self._check_fixed_signature(
                node, name, ('start', 'stop', 'step'), 1)
        if name == 'PSum':
            self._check_fixed_signature(
                node, name, ('n', 'total', 'lim'), 2)
        if name == 'PIndex':
            self._check_fixed_signature(node, name, ('length',), 0)
        if name == 'PwRand':
            self._check_fixed_signature(
                node, name, ('values', 'weights', 'length', 'seed'), 2)
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
        if name in {'PRand', 'PxRand', 'PWhite'}:
            parameters = (('lo', 'hi') if name == 'PWhite'
                          else ('start', 'stop'))
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
            if name in {'PRand', 'PxRand'} and not positional.intersection({'start'}):
                self.add(node.lineno,
                         name + ' requires a start value or source pattern.')
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
            effect_name = next(
                (keyword.value.value for keyword in node.keywords
                 if keyword.arg == 'effect' and
                 isinstance(keyword.value, ast.Constant) and
                 isinstance(keyword.value.value, str)),
                None)
            for keyword in node.keywords:
                if keyword.arg != 'effect':
                    continue
                if not isinstance(keyword.value, ast.Constant) or not isinstance(keyword.value.value, str):
                    self.add(keyword.lineno or node.lineno,
                             'effect= requires a literal installed player-effect name.')
                elif keyword.value.value not in {'AirwindowsTapeDelay2', 'AirwindowsEchojam', 'JPverb', 'Greyhole', 'Chorus', 'Flanger', 'Phaser', 'Tremolo', 'LimutPingPong', 'LimutDuck'}:
                    self.add(keyword.lineno or node.lineno,
                             'effect= supports AirwindowsTapeDelay2, AirwindowsEchojam, JPverb, Greyhole, Chorus, Flanger, Phaser, Tremolo, LimutPingPong or LimutDuck.')
                elif keyword.value.value not in _USER_SYNTH_NAMES:
                    self.add(keyword.lineno or node.lineno,
                             'effect= requires the matching player-effect SynthDef in Portable/SynthDefs.')
            if effect_name in _COMPAT_PLAYER_EFFECT_CONTROLS:
                supported_controls = _COMPAT_PLAYER_EFFECT_CONTROLS[effect_name]
                for keyword in node.keywords:
                    if (keyword.arg and keyword.arg.startswith('fx_') and
                            keyword.arg not in supported_controls):
                        self.add(
                            keyword.lineno or node.lineno,
                            keyword.arg + ' is not a control on ' +
                            effect_name + '; supported controls: ' +
                            ', '.join(sorted(supported_controls)) + '.')
            chain = next((kw for kw in node.keywords
                          if kw.arg == 'effects'), None)
            if chain is not None:
                if any(kw.arg == 'effect' or
                       (kw.arg and kw.arg.startswith('fx_'))
                       for kw in node.keywords):
                    self.add(node.lineno,
                             'Use effects=[(name, controls), ...] without effect= or standalone fx_* controls.')
                if not isinstance(chain.value, (ast.List, ast.Tuple)):
                    self.add(chain.lineno,
                             'effects= requires a literal list of (installed name, controls dictionary) pairs.')
                else:
                    if len(chain.value.elts) > 4:
                        self.add(chain.lineno,
                                 'A player effect chain is limited to four inserts.')
                    for entry in chain.value.elts:
                        if (not isinstance(entry, (ast.List, ast.Tuple)) or
                                len(entry.elts) != 2):
                            self.add(entry.lineno,
                                     'Each effect-chain entry must be (name, controls dictionary).')
                            continue
                        effectName, controls = entry.elts
                        if (not isinstance(effectName, ast.Constant) or
                                effectName.value not in {
                                    'AirwindowsTapeDelay2', 'AirwindowsEchojam',
                                    'JPverb', 'Greyhole', 'Chorus', 'Flanger',
                                    'Phaser', 'Tremolo', 'LimutPingPong',
                                    'LimutDuck'}):
                            self.add(entry.lineno, 'Unsupported effect-chain name.')
                        elif effectName.value not in _USER_SYNTH_NAMES:
                            self.add(entry.lineno,
                                     'effects= requires ' + effectName.value +
                                     ' in Portable/SynthDefs.')
                        if not isinstance(controls, ast.Dict):
                            self.add(entry.lineno,
                                     'Effect-chain controls must be a literal dictionary.')
                        else:
                            invalid_control = any(
                                not isinstance(key, ast.Constant) or
                                not isinstance(key.value, str) or
                                key.value not in _COMPAT_WAVE_KEYWORDS or
                                not key.value.startswith('fx_')
                                for key in controls.keys)
                            if invalid_control:
                                self.add(
                                    entry.lineno,
                                    'Effect-chain dictionaries accept supported fx_* control names only.')
                            elif (isinstance(effectName, ast.Constant) and
                                  isinstance(effectName.value, str) and
                                  effectName.value in
                                  _COMPAT_PLAYER_EFFECT_CONTROLS):
                                supported_controls = (
                                    _COMPAT_PLAYER_EFFECT_CONTROLS[
                                        effectName.value])
                                for key in controls.keys:
                                    if key.value not in supported_controls:
                                        self.add(
                                            key.lineno or entry.lineno,
                                            key.value + ' is not a control on ' +
                                            effectName.value +
                                            '; supported controls: ' +
                                            ', '.join(sorted(supported_controls)) +
                                            '.')
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
        if isinstance(node.value, ast.Name) and node.value.id == 'Clock' and node.attr not in {'bpm', 'meter', 'swing', 'clear'}:
            self.add(node.lineno, "Clock." + node.attr + ' is not supported; use Clock.bpm, fixed 4/4 Clock.meter, Clock.swing(...), or Clock.clear().')
        if isinstance(node.value, ast.Name) and node.value.id == 'Seed' and node.attr != 'default':
            self.add(node.lineno, 'Seed only supports Seed.default in the embedded random-pattern API.')
        if (isinstance(node.value, ast.Name) and node.value.id == 'Clock' and
                node.attr in {'swing', 'clear'} and isinstance(node.ctx, ast.Store)):
            self.add(node.lineno, 'Clock.' + node.attr +
                     ' is a method; call it, not assign to it.')
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
        is_pattern_value = self._finite_pattern_expression(node.value)
        is_pvar_value = self._is_pvar_expression(node.value)
        for target in node.targets:
            if isinstance(target, ast.Name):
                if is_pattern_value:
                    self.pattern_names.add(target.id)
                else:
                    self.pattern_names.discard(target.id)
                if is_pvar_value:
                    self.pvar_names.add(target.id)
                else:
                    self.pvar_names.discard(target.id)
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
_dexter_effect_control_values = {}
_dexter_effect_control_variables = {}
_dexter_player_effect_chains = {}
_dexter_note_effect_control_variables = {}
_dexter_bitcrusher_controls = {}
_dexter_shuffle_counter = 0
_dexter_random_counter = 0
_dexter_default_seed = None
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
    global _dexter_bpm, _dexter_bpm_control
    control = bpm if isinstance(bpm, _DexterTimeVar) else None
    values = control.values if control is not None else [float(bpm)]
    if any(value != value or value in (float('inf'), float('-inf')) or
           value < 0.001 for value in values):
        raise ValueError('tempo must be finite and at least 0.001 BPM')
    if any(abs(_dexter_swing_seconds * value / 60.0) >= 0.49 for value in values):
        raise ValueError('Clock.swing would shift notes by 0.49 beats or more at this tempo')
    _dexter_bpm = float(values[0])
    _dexter_bpm_control = control

class _FoxDotClock:
    def __getattr__(self, name):
        if name == 'bpm':
            return _dexter_bpm_control if _dexter_bpm_control is not None else _dexter_bpm
        if name == 'meter':
            return (4, 4)
        raise ValueError('Clock.' + str(name) + ' is not supported in the embedded clock')
    def swing(self, amount=0.1):
        global _dexter_swing_seconds
        amount = float(amount)
        if amount != amount or amount in (float('inf'), float('-inf')):
            raise ValueError('Clock.swing amount must be finite')
        nudge = amount * (_dexter_bpm / 120.0)
        tempos = (_dexter_bpm_control.values if _dexter_bpm_control is not None
                  else [_dexter_bpm])
        if any(abs(nudge * bpm / 60.0) >= 0.49 for bpm in tempos):
            raise ValueError('Clock.swing would shift notes by 0.49 beats or more at this tempo')
        _dexter_swing_seconds = nudge
        for _player in _dexter_updates:
            _update = _dexter_updates[_player]
            _dexter_updates[_player] = _update[:-1] + (_dexter_swing_seconds,)
    def clear(self):
        for _player in (p1, p2, p3, p4, p5, p6, p7, p8):
            _player.stop()
    def __setattr__(self, name, value):
        if name == 'bpm':
            tempo(value)
            return
        if name == 'meter' and tuple(value) == (4, 4):
            return
        raise AttributeError('Clock.' + str(name) + ' is not supported in the embedded clock')

Clock = _FoxDotClock()

class _FoxDotSeed:
    def __getattr__(self, name):
        if name == 'default':
            return _dexter_default_seed
        raise AttributeError('Seed.' + str(name) + ' is not supported')
    def __setattr__(self, name, value):
        global _dexter_default_seed
        if name != 'default':
            raise AttributeError('Seed.' + str(name) + ' is not supported')
        if value is not None:
            if isinstance(value, bool) or not isinstance(value, int):
                raise TypeError('Seed.default must be a whole number or None')
            if not 0 <= value <= 0xffffffff:
                raise ValueError('Seed.default must be from 0 to 4294967295 or None')
            value = int(value)
        _dexter_default_seed = value

Seed = _FoxDotSeed()

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

def _safe_pattern_mapper(mapper):
    safe_builtins = _dexter_pure_map_builtins
    if any(mapper is function for function in safe_builtins.values()):
        return True
    if not isinstance(mapper, _dexter_function_type):
        return False
    code = mapper.__code__
    if (code.co_argcount != 1 or code.co_kwonlyargcount or code.co_freevars or
            code.co_flags & (0x04 | 0x08 | 0x20 | 0x80) or
            len(code.co_code) > 256):
        return False
    allowed_globals = {'abs', 'bool', 'float', 'int', 'len', 'max', 'min',
                       'round', 'str'}
    allowed_opcodes = {
        'BINARY_OP', 'CALL', 'COMPARE_OP', 'CONTAINS_OP', 'COPY',
        'IS_OP', 'JUMP_FORWARD', 'JUMP_IF_FALSE_OR_POP',
        'JUMP_IF_TRUE_OR_POP', 'LOAD_CONST', 'LOAD_FAST', 'LOAD_GLOBAL',
        'NOP', 'POP_JUMP_FORWARD_IF_FALSE', 'POP_JUMP_FORWARD_IF_TRUE',
        'POP_TOP', 'PRECALL', 'PUSH_NULL', 'RESUME', 'RETURN_VALUE',
        'SWAP', 'TO_BOOL', 'UNARY_NEGATIVE', 'UNARY_NOT', 'UNARY_POSITIVE'
    }
    try:
        instructions = list(_dexter_dis.get_instructions(mapper))
    except Exception:
        return False
    if len(instructions) > 64:
        return False
    for instruction in instructions:
        if instruction.opname not in allowed_opcodes:
            return False
        if (instruction.opname == 'LOAD_GLOBAL' and
                instruction.argval not in allowed_globals):
            return False
        if instruction.opname == 'LOAD_GLOBAL':
            function_globals = mapper.__globals__
            if instruction.argval in function_globals:
                resolved = function_globals[instruction.argval]
            else:
                builtin_scope = function_globals.get('__builtins__')
                if isinstance(builtin_scope, dict):
                    resolved = builtin_scope.get(instruction.argval)
                elif builtin_scope is _dexter_builtin_module:
                    resolved = builtin_scope.__dict__.get(instruction.argval)
                else:
                    return False
            if resolved is not safe_builtins[instruction.argval]:
                return False
        if (instruction.opname == 'BINARY_OP' and
                instruction.argrepr not in ('+', '-', '*', '/', '//', '%')):
            return False
    return True

class _FoxDotMapMissing:
    pass

_FOXDOT_MAP_MISSING = _FoxDotMapMissing()

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

    def _comparison_apply(self, left, right, operation, method_name,
                          budget, depth=0):
        if depth > 16:
            raise ValueError(method_name + ' nesting is limited to 16 levels')
        left_group = isinstance(left, (list, tuple))
        right_group = isinstance(right, (list, tuple))
        if left_group or right_group:
            if left_group and right_group:
                if len(left) != len(right):
                    raise ValueError(
                        method_name + ' chord groups must have matching sizes')
                source = left
                pairs = zip(left, right)
            elif left_group:
                source = left
                pairs = ((item, right) for item in left)
            else:
                source = right
                pairs = ((left, item) for item in right)
            values = [
                self._comparison_apply(a, b, operation, method_name,
                                      budget, depth + 1)
                for a, b in pairs
            ]
            if isinstance(source, _FoxDotFinitePattern):
                return _FoxDotComparisonPattern(values)
            if isinstance(source, _FoxDotChord):
                return _FoxDotChord(*values)
            return tuple(values) if isinstance(source, tuple) else values

        budget[0] += 1
        if budget[0] > 256:
            raise ValueError(method_name + ' output is limited to 256 values')
        if left is None or right is None:
            if method_name in ('eq', 'ne'):
                return int(operation(left, right))
            return None
        left_number = _finite_note_number(left)
        right_number = _finite_note_number(right)
        left_text = isinstance(left, str) and len(left) <= 256
        right_text = isinstance(right, str) and len(right) <= 256
        if left_number and right_number:
            return int(operation(left, right))
        if left_text and right_text:
            return int(operation(left, right))
        if method_name in ('eq', 'ne') and (
                (left_number and right_text) or
                (left_text and right_number)):
            return int(operation(left, right))
        raise TypeError(
            method_name + ' compares finite numeric notes or bounded text')

    def _binary(self, other, operation, reflected=False):
        if not len(self):
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

    def add(self, other):
        """FoxDot method form of pattern addition."""
        return self + other

    def __radd__(self, other):
        return self._binary(other, lambda a, b: a + b, True)

    def __abs__(self):
        budget = [0]

        def absolute(item, depth):
            if depth > 16:
                raise ValueError('P.abs is limited to 16 nested levels')
            if item is None:
                return None
            if isinstance(item, _FoxDotFinitePattern):
                if len(item) > 256:
                    raise ValueError('P.abs nested patterns are limited to 256 items')
                return _FoxDotFinitePattern([
                    absolute(value, depth + 1) for value in item])
            if isinstance(item, _FoxDotChord):
                return _FoxDotChord(*[
                    absolute(value, depth + 1) for value in item])
            if isinstance(item, tuple):
                if len(item) > 256:
                    raise ValueError('P.abs groups are limited to 256 items')
                return tuple(absolute(value, depth + 1) for value in item)
            if isinstance(item, list):
                if len(item) > 256:
                    raise ValueError('P.abs groups are limited to 256 items')
                return [absolute(value, depth + 1) for value in item]
            if not _finite_note_number(item):
                raise TypeError('P.abs requires finite numeric notes or rests')
            budget[0] += 1
            if budget[0] > 256:
                raise ValueError('P.abs output is limited to 256 notes')
            return item if item >= 0 else -item

        return _FoxDotFinitePattern([
            absolute(item, 0) for item in self])

    def abs(self):
        return self.__abs__()

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

    def __eq__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left == right, 'eq')

    def __ne__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left != right, 'ne')

    def __lt__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left < right, 'lt')

    def __le__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left <= right, 'le')

    def __gt__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left > right, 'gt')

    def __ge__(self, other):
        return self._compare_pattern(
            other, lambda left, right: left >= right, 'ge')

    def __invert__(self):
        return self.mirror()

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

    def i_rotate(self, amount=1):
        """Rotate this Pattern in place using FoxDot's slice-based semantics."""
        if isinstance(amount, bool) or not isinstance(amount, int):
            raise TypeError('i_rotate amount must be a whole number')
        self._validate_mutation_values(self)
        values = list(self)
        list.__setitem__(
            self, slice(None), values[amount:] + values[:amount])
        return self

    def i_reverse(self):
        """Reverse this bounded Pattern in place and return it."""
        self._validate_mutation_values(self)
        list.reverse(self)
        return self

    def i_sort(self):
        """Sort comparable Pattern items in place without partial mutation."""
        self._validate_mutation_values(self)
        try:
            values = sorted(self)
        except TypeError:
            raise TypeError('i_sort requires mutually comparable Pattern items')
        list.__setitem__(self, slice(None), values)
        return self

    def i_shuf(self):
        """Shuffle this Pattern in place using the bounded seeded shuffle."""
        shuffled = PShuf(self)
        list.__setitem__(self, slice(None), list(shuffled))
        return self

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

    def _repeat_finite_pattern(self, count, method_name):
        if isinstance(count, bool) or not isinstance(count, int):
            raise TypeError(method_name + ' count must be a whole number')
        if count < 0 or count > 256:
            raise ValueError(method_name + ' count must be from 0 to 256')
        if len(self) * count > 256:
            raise ValueError(method_name + ' output is limited to 256 items')
        return _FoxDotFinitePattern(list(self) * count)

    def duplicate(self, count):
        """Repeat top-level items without flattening nested groups."""
        return self._repeat_finite_pattern(count, 'duplicate')

    def iter(self, count):
        """Cycle the finite pattern count times, preserving nested items."""
        return self._repeat_finite_pattern(count, 'iter')

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

    def sample(self, count, seed=None):
        if isinstance(count, bool) or not isinstance(count, int):
            raise TypeError('sample count must be a whole number')
        if not 0 <= count <= len(self):
            raise ValueError('sample count must be between 0 and source length')
        if count > 256:
            raise ValueError('sample output is limited to 256 items')
        state = _random_pattern_seed(seed, 'sample')
        if count == 0:
            return _FoxDotFinitePattern()
        shuffled, _ = _seeded_pattern_shuffle(self, state)
        return _FoxDotFinitePattern(shuffled[:count])

    def deep_shuffle(self, count=1, seed=None):
        if isinstance(count, bool) or not isinstance(count, int):
            raise TypeError('deep_shuffle count must be a whole number from 0 to 256')
        if not 0 <= count <= 256:
            raise ValueError('deep_shuffle count must be a whole number from 0 to 256')
        if len(self) * count > 256:
            raise ValueError('deep_shuffle output is limited to 256 items')
        state = _random_pattern_seed(seed, 'deep_shuffle')
        if count == 0 or not self:
            return _FoxDotFinitePattern()
        result = []
        for _ in range(count):
            shuffled, state = _seeded_pattern_shuffle(self, state)
            result.extend(shuffled)
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
        source = _FoxD   << "            if _synth == 57:\n"
             << "                _sample_control, _rate_control = _dexter_scope['_dexter_sample_controls'].get(_player, ('-', '-'))\n"
             << "                _f.write('Q' + chr(9) + str(_player) + chr(9) + _sample_control + chr(9) + _rate_control + chr(10))\n"
            << "            for _event_index, _e in enumerate(_events):\n"
               << "                _fx = _e[10:18] if len(_e) >= 18 else (0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)\n"
               << "                _cut = _e[18:19] if len(_e) >= 19 else (1.0,)\n"
               << "                _extra_note_fx = _e[19:22] if len(_e) >= 22 else (0.0, -1.0, 0.0)\n"
               << "                _f.write('N' + chr(9) + chr(9).join(map(str, _e[:7] + _fx + _cut + _extra_note_fx)) + chr(10))\n"
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
        char tempoRecord = 0;
        int tempoPlayer = -1;
        if (!(input >> tempoRecord >> tempoPlayer >> update.bpmControlToken) ||
            tempoRecord != 'T' || tempoPlayer != update.playerIndex ||
            update.bpmControlToken.size() > 256) {
            error = "Python returned malformed Clock.bpm automation.";
            return false;
        }
        if (update.bpmControlToken != "-") {
            const auto& token = update.bpmControlToken;
            const auto delimiter = token.find(':', 2);
            if (token.size() < 5 || (token[0] != 'v' && token[0] != 'l') ||
                token[1] != ':' || delimiter == std::string::npos ||
                delimiter == 2 || delimiter + 1 == token.size() ||
                token.find(':', delimiter + 1) != std::string::npos) {
                error = "Python returned malformed Clock.bpm automation.";
                return false;
            }
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
        const auto validPlayerEffectControlToken = [](const std::string& token,
                                                       double minimum,
                                                       double maximum) {
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
            const auto parseList = [](const std::string& text, double minimum,
                                      double maximum, bool positive,
                                      std::vector<double>& output) {
                std::istringstream values(text);
                std::string part;
                while (std::getline(values, part, ',')) {
                    if (part.empty() || output.size() >= 8)
                        return false;
                    try {
                        std::size_t used = 0;
                        const double value = std::stod(part, &used);
                        if (used != part.size() || !std::isfinite(value) ||
                            (positive ? value <= 0.0 || value > maximum
                                      : value < minimum || value > maximum))
                            return false;
                        output.push_back(value);
                    } catch (...) {
                        return false;
                    }
                }
                return !output.empty();
            };
            std::vector<double> values;
            std::vector<double> durations;
            return parseList(token.substr(2, middle - 2), minimum, maximum, false,
                             values) &&
                   parseList(token.substr(middle + 1), 0.0, 4096.0, true,
                             durations) &&
                   values.size() == durations.size();
        };
        char modulationRecord = 0;
        int modulationPlayer = -1;
        if (!(input >> modulationRecord >> modulationPlayer >> update.bpnoise >>
              update.swell >> update.shape >> update.drive >>
              update.bpnoiseControlToken >> update.swellControlToken >>
              update.shapeControlToken >> update.driveControlToken >>
              update.bits >> update.bitsControlToken >>
              update.crush >> update.crushControlToken) ||
            modulationRecord != 'G' || modulationPlayer != update.playerIndex ||
            !std::isfinite(update.bpnoise) || update.bpnoise < 0.0 ||
            update.bpnoise > 10.0 || !std::isfinite(update.swell) ||
            update.swell < 0.0 || update.swell > 4.0 ||
            !std::isfinite(update.shape) || update.shape < 0.0 ||
            update.shape > 4.0 || !std::isfinite(update.drive) ||
            update.drive < 0.0 || update.drive > 4.0 ||
            !std::isfinite(update.bits) || update.bits < 0.0 ||
            update.bits > 24.0 ||
            !std::isfinite(update.crush) || update.crush < 1.0 ||
            update.crush > 128.0 ||
            !validPlayerEffectControlToken(update.shapeControlToken, 0.0, 4.0) ||
            !validPlayerEffectControlToken(update.driveControlToken, 0.0, 4.0) ||
            !validPlayerEffectControlToken(update.bitsControlToken, 0.0, 24.0) ||
            !validPlayerEffectControlToken(update.crushControlToken, 1.0, 128.0)) {
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
            error = "Python returned an invalid persistent player-effect selection.";
            return false;
        }
        char effectValuesRecord = 0;
        int effectValuesPlayer = -1;
        if (!(input >> effectValuesRecord >> effectValuesPlayer) ||
            effectValuesRecord != 'W' ||
            effectValuesPlayer != update.playerIndex) {
            error = "Python returned malformed persistent-effect control values.";
            return false;
        }
        for (auto& value : update.playerEffectControlValues) {
            if (!(input >> value) || !std::isfinite(value) ||
                value < 0.0 || value > 1.0) {
                error = "Python returned invalid normalized player-effect controls.";
                return false;
            }
        }
        char effectControlsRecord = 0;
        int effectControlsPlayer = -1;
        if (!(input >> effectControlsRecord >> effectControlsPlayer) ||
            effectControlsRecord != 'X' ||
            effectControlsPlayer != update.playerIndex) {
            error = "Python returned malformed persistent-effect controls.";
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
                error = "Python returned malformed persistent effect-control modulation.";
                return false;
            }
            if (token != "-" && update.playerEffectUserIndex < 0) {
                error = "Python returned persistent-effect modulation without an effect selection.";
                return false;
            }
        }
        update.playerEffects.clear();
        if (update.playerEffectUserIndex >= 0)
            update.playerEffects.push_back({
                update.playerEffectUserIndex, update.playerEffectControlValues,
                update.effectControlTokens});
        char chainRecord = 0;
        int chainPlayer = -1;
        std::size_t extraEffectCount = 0;
        if (!(input >> chainRecord >> chainPlayer >> extraEffectCount) ||
            chainRecord != 'J' || chainPlayer != update.playerIndex ||
            extraEffectCount >= kMaxPlayerEffectChain ||
            (extraEffectCount > 0 && update.playerEffectUserIndex < 0)) {
            error = "Python returned an invalid persistent effect-chain header.";
            return false;
        }
        for (std::size_t slot = 0; slot < extraEffectCount; ++slot) {
            PythonPlayerEffect effect;
            if (!(input >> effect.userSynthIndex) ||
                effect.userSynthIndex < 0 ||
                static_cast<std::size_t>(effect.userSynthIndex) >= userSynthCount) {
                error = "Python returned an invalid chained effect selection.";
                return false;
            }
            for (auto& value : effect.controlValues) {
                if (!(input >> value) || !std::isfinite(value) ||
                    value < 0.0 || value > 1.0) {
                    error = "Python returned invalid chained effect controls.";
                    return false;
                }
            }
            for (auto& token : effect.controlTokens) {
                if (!(input >> token) || !validEffectControlToken(token)) {
                    error = "Python returned malformed chained effect modulation.";
                    return false;
                }
            }
            update.playerEffects.push_back(std::move(effect));
        }
        const auto validNoteEffectControlToken =
            [](const std::string& token, double minimum, double maximum) {
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
                const auto parseList = [](const std::string& text,
                                          double low, double high,
                                          bool durations,
                                          std::size_t& count) {
                    if (text.empty() || text.front() == ',' ||
                        text.back() == ',' || text.find(",,") != std::string::npos)
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
                                (durations ? value <= 0.0 || value > 4096.0
                                           : value < low || value > high))
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
                return parseList(token.substr(2, middle - 2), minimum,
                                 maximum, false, valueCount) &&
                       parseList(token.substr(middle + 1), 0.0, 4096.0,
                                 true, durationCount) &&
                       (durationCount == 1 || durationCount == valueCount);
            };
        char noteEffectRecord = 0;
        int noteEffectPlayer = -1;
        if (!(input >> noteEffectRecord >> noteEffectPlayer) ||
            noteEffectRecord != 'Y' || noteEffectPlayer != update.playerIndex) {
            error = "Python returned malformed note-effect time-var controls.";
            return false;
        }
        constexpr std::array<double, 12> noteEffectMinimum{
            0.0, -1.0, -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, -24.0};
        constexpr std::array<double, 12> noteEffectMaximum{
            10.0, 4.0, 4.0, 1.0, 4.0, 1.0, 32.0, 40.0, 1.0, 32.0, 7.0, 24.0};
        for (std::size_t index = 0;
             index < update.noteEffectControlTokens.size(); ++index) {
            auto& token = update.noteEffectControlTokens[index];
            if (!(input >> token)) {
                error = "Python returned an incomplete note-effect control list.";
                return false;
            }
            if (!validNoteEffectControlToken(
                    token, noteEffectMinimum[index], noteEffectMaximum[index])) {
                error = "Python returned invalid note-effect time-var control " +
                        std::to_string(index) + ": " + token;
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
                  event.midiNote >> event.velocity >> event.attackSeconds >>
                  event.releaseSeconds >> event.delayBeats >> event.vibDepth >>
                  event.slide >> event.slideFrom >> event.slideDelay >>
                  event.bend >> event.bendDelay >> event.chop >> event.spin >>
                  event.cut >> event.coarse >> event.formant >> event.pshift) ||
                noteRecord != 'N' ||
                !std::isfinite(event.beat) || !std::isfinite(event.durationBeats) ||
                !std::isfinite(event.delayBeats) || event.delayBeats < 0.0 || event.delayBeats > 15.0 ||
                !std::isfinite(event.vibDepth) || event.vibDepth < 0.0 ||
                event.vibDepth > 10.0 ||
                !std::isfinite(event.slide) || event.slide < -1.0 ||
                event.slide > 4.0 ||
                !std::isfinite(event.slideFrom) || event.slideFrom < -1.0 ||
                event.slideFrom > 4.0 ||
                !std::isfinite(event.slideDelay) || event.slideDelay < 0.0 ||
                event.slideDelay > 1.0 ||
                !std::isfinite(event.bend) || event.bend < -1.0 ||
                event.bend > 4.0 ||
                !std::isfinite(event.bendDelay) || event.bendDelay < 0.0 ||
                event.bendDelay > 1.0 ||
                !std::isfinite(event.chop) || event.chop < 0.0 ||
                event.chop > 32.0 ||
                !std::isfinite(event.spin) || event.spin < 0.0 ||
                event.spin > 40.0 ||
                !std::isfinite(event.cut) || event.cut < 0.0 ||
                event.cut > 1.0 ||
                !std::isfinite(event.coarse) || event.coarse < 0.0 ||
                event.coarse > 32.0 ||
                !std::isfinite(event.formant) || event.formant < -1.0 ||
                event.formant > 7.0 ||
                !std::isfinite(event.pshift) || event.pshift < -24.0 ||
                event.pshift > 24.0 ||
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
