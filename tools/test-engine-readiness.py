#!/usr/bin/env python3
"""Test actual host setup and initializeWorld entry with a fake SC backend.

This does not validate SC loading/audio. Windows' required validator exercises
the real engine; this test isolates host-order/retry/locking regressions.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "spikes/vst3-callback/processor.cpp").read_text()
configure_start = source.index("    void configureHostProcessing(")
start = source.index("    bool initializeWorld()")
end = source.index("        std::lock_guard<std::mutex> lifecycleLock", start)
entry = source[start:end]
configure = source[configure_start:start]
setup = source[source.index("    tresult PLUGIN_API setupProcessing("):
               source.index("    tresult PLUGIN_API terminate(")]
assert "initializeWorld" not in setup, "Host setup must not start SuperCollider"
assert "std::isfinite(setup.sampleRate)" in source
assert "if (!initializeWorld())" in source[source.index("    bool prepareSynthGraphs("):start]
process_prefix = source[source.index("    tresult PLUGIN_API process("):
                        source.index("        std::unique_lock<std::mutex> patternLock")]
assert "if (!world_) {\n            output.silenceFlags = ~uint64{0};\n            return kResultOk;" in process_prefix
code = r"""
#include <cassert>
#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#define PLUGIN_API
using tresult = int;
constexpr int kResultOk = 0, kInvalidArgument = 1;
struct ProcessSetup { double sampleRate; };
struct SingleComponentEffect {
    virtual tresult setupProcessing(ProcessSetup&) { return kResultOk; }
};
struct LifecycleTrace {
    LifecycleTrace(const char*, const void*) {}
};
struct Engine : SingleComponentEffect {
    std::mutex worldAccessMutex_;
    void* world_ = nullptr;
    double sampleRate_ = 48000;
    bool hostProcessingConfigured_ = false;
    std::string worldInitializationError_, status;
    int initializations = 0, resets = 0;
    bool backendFails = false;
    bool diskCleanupFailed_ = false;
    void setEditorStatus(std::string text) { status = std::move(text); }
    void stopWorldLocked() { world_ = nullptr; ++resets; }
"""
code += setup
code += configure
code += entry
code += r"""
        ++initializations;
        if (backendFails) return fail("Required UGen test backend is unavailable.");
        world_ = this;
        worldInitializationError_.clear();
        setEditorStatus("SuperCollider ready.");
        return true;
    }
};
int main() {
    Engine engine;
    assert(!engine.initializeWorld());
    assert(engine.initializations == 0 && engine.world_ == nullptr);
    assert(engine.status.find("host has not configured") != std::string::npos);
    ProcessSetup invalid{std::numeric_limits<double>::quiet_NaN()};
    assert(engine.setupProcessing(invalid) == kInvalidArgument);
    assert(!engine.hostProcessingConfigured_);
    ProcessSetup setup{44100};
    engine.backendFails = true;
    assert(engine.setupProcessing(setup) == kResultOk);
    assert(engine.initializations == 0 && !engine.world_);
    assert(engine.hostProcessingConfigured_ && engine.sampleRate_ == 44100);
    assert(!engine.initializeWorld());
    assert(engine.initializations == 1 && engine.sampleRate_ == 44100);
    assert(engine.worldInitializationError_.find("UGen") != std::string::npos);
    engine.backendFails = false;
    assert(engine.initializeWorld());
    assert(engine.initializations == 2 && engine.sampleRate_ == 44100);
    assert(engine.worldInitializationError_.empty());
    assert(engine.status == "SuperCollider ready.");
    const auto resets = engine.resets;
    assert(engine.setupProcessing(setup) == kResultOk);
    assert(engine.initializeWorld());
    assert(engine.initializations == 2 && engine.resets == resets);
    setup.sampleRate = 96000;
    assert(engine.setupProcessing(setup) == kResultOk);
    assert(!engine.world_ && engine.initializations == 2);
    assert(engine.resets == resets + 1);
    assert(engine.initializeWorld());
    assert(engine.sampleRate_ == 96000 && engine.initializations == 3);
    std::thread host([&] {
        for (int i = 0; i < 100; ++i) {
            ProcessSetup changing{i % 2 ? 44100.0 : 48000.0};
            assert(engine.setupProcessing(changing) == kResultOk);
        }
    });
    std::thread worker([&] {
        for (int i = 0; i < 100; ++i) assert(engine.initializeWorld());
    });
    host.join();
    worker.join();
    assert(engine.initializeWorld() && engine.worldInitializationError_.empty());
    engine.world_ = nullptr;
    engine.diskCleanupFailed_ = true;
    assert(!engine.initializeWorld());
    assert(engine.status.find("teardown") != std::string::npos);
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "readiness.cpp", directory / "readiness"
    unit.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    str(unit), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("PASS: actual host setup does not initialize SC; deferred retry, silent idle, invalid rates, and concurrent rate changes")
