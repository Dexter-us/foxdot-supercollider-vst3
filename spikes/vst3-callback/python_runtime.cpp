#include "python_runtime.h"

#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <sstream>

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
    if not math.isfinite(bpm) or not 20.0 <= bpm <= 300.0:
        raise ValueError('tempo must be from 20 to 300 BPM')
    _dexter_bpm = bpm

def play(notes, step=1.0, duration=None, velocity=100, offset=0.0):
    global _dexter_length, _dexter_slots
    if isinstance(notes, (int, float)):
        notes = [notes]
    notes = list(notes)
    step = float(step)
    duration = step if duration is None else float(duration)
    offset = float(offset)
    velocity = int(velocity)
    if not math.isfinite(step) or not math.isfinite(duration) or not math.isfinite(offset):
        raise ValueError('step, duration, and offset must be finite numbers')
    if step < 0.1 or duration < 0.02 or duration > step:
        raise ValueError('step must be at least 0.1 beats; duration must be 0.02 beats or longer and fit within the step')
    if offset < 0.0 or offset + len(notes) * step > 4096:
        raise ValueError('offset and pattern length must fit within 4096 beats')
    if not 1 <= velocity <= 127:
        raise ValueError('velocity must be 1..127')
    if _dexter_slots + len(notes) > 256:
        raise ValueError('a pattern is limited to 256 note slots')
    for i, note in enumerate(notes):
        if note is None:
            continue
        note = int(note)
        if not 0 <= note <= 127:
            raise ValueError('MIDI notes must be 0..127')
        start = offset + i * step
        if any(start < old_start + old_duration and start + duration > old_start
               for old_start, old_duration, _, _ in _dexter_events):
            raise ValueError('overlapping notes are not supported in this monophonic prototype')
        _dexter_events.append((start, duration, note, velocity))
    _dexter_slots += len(notes)
    _dexter_length = max(_dexter_length, offset + len(notes) * step)
)PY";

    std::ostringstream script;
    script << "import builtins, math, traceback, sys, time\n"
           << "_dexter_scope = globals().setdefault(" << pythonLiteral(namespaceName) << ", {})\n"
           << "_dexter_scope.update({'_dexter_events': [], '_dexter_bpm': 120.0, "
           << "'_dexter_length': 1.0, '_dexter_slots': 0, 'math': math})\n"
           << "_dexter_scope['__builtins__'] = {name: getattr(builtins, name) for name in "
           << "('abs', 'all', 'any', 'bool', 'dict', 'enumerate', 'filter', 'float', "
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
           << "        _f.write('OK\\t{}\\t{}\\t{}\\n'.format(_dexter_scope['_dexter_bpm'], "
           << "_dexter_scope['_dexter_length'], len(_dexter_scope['_dexter_events'])))\n"
           << "        for _e in _dexter_scope['_dexter_events']:\n"
           << "            _f.write('{}\\t{}\\t{}\\t{}\\n'.format(*_e))\n"
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
    std::size_t eventCount = 0;
    if (!(fields >> marker >> pattern.bpm >> pattern.lengthBeats >> eventCount) ||
        marker != "OK" || eventCount > 256 ||
        !std::isfinite(pattern.bpm) || !std::isfinite(pattern.lengthBeats) ||
        pattern.bpm < 20.0 || pattern.bpm > 300.0 ||
        pattern.lengthBeats <= 0.0) {
        error = "Python returned an invalid pattern.";
        return false;
    }

    pattern.events.clear();
    pattern.events.reserve(eventCount);
    for (std::size_t i = 0; i < eventCount; ++i) {
        PythonNoteEvent event;
        if (!(input >> event.beat >> event.durationBeats >> event.midiNote >> event.velocity) ||
            !std::isfinite(event.beat) || !std::isfinite(event.durationBeats) ||
            event.beat < 0.0 || event.durationBeats <= 0.0 ||
            event.midiNote < 0 || event.midiNote > 127 ||
            event.velocity < 1 || event.velocity > 127) {
            error = "Python returned a malformed note event.";
            return false;
        }
        pattern.events.push_back(event);
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
