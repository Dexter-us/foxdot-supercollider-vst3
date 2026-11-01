#!/usr/bin/env python3
"""Test the actual initializeWorld entry/retry logic with a fake SC backend.

This does not validate SC loading/audio. Windows' required validator exercises
the real engine; this test isolates host-order/retry/locking regressions.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "spikes/vst3-callback/processor.cpp").read_text()
start = source.index("    bool initializeWorld(double hostSampleRate")
end = source.index("        std::lock_guard<std::mutex> lifecycleLock", start)
entry = source[start:end]
assert "std::isfinite(setup.sampleRate)" in source
assert "if (!initializeWorld())" in source[source.index("    bool prepareSynthGraphs("):start]
code = r"""
#include <cassert>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
struct LifecycleTrace {
    LifecycleTrace(const char*, const void*) {}
};
struct Engine {
    std::mutex worldAccessMutex_;
    void* world_ = nullptr;
    double sampleRate_ = 48000;
    bool hostProcessingConfigured_ = false;
    std::string worldInitializationError_, status;
    int initializations = 0, resets = 0;
    bool backendFails = false;
    void setEditorStatus(std::string text) { status = std::move(text); }
    void stopWorldLocked() { world_ = nullptr; ++resets; }
"""
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
    engine.backendFails = true;
    assert(!engine.initializeWorld(44100));
    assert(engine.initializations == 1 && engine.sampleRate_ == 44100);
    assert(engine.worldInitializationError_.find("UGen") != std::string::npos);
    engine.backendFails = false;
    assert(engine.initializeWorld());
    assert(engine.initializations == 2 && engine.sampleRate_ == 44100);
    assert(engine.worldInitializationError_.empty());
    assert(engine.status == "SuperCollider ready.");
    const auto resets = engine.resets;
    assert(engine.initializeWorld() && engine.initializeWorld(44100));
    assert(engine.initializations == 2 && engine.resets == resets);
    assert(engine.initializeWorld(96000));
    assert(engine.sampleRate_ == 96000 && engine.initializations == 3);
    std::thread host([&] {
        for (int i = 0; i < 100; ++i) assert(engine.initializeWorld(i % 2 ? 44100 : 48000));
    });
    std::thread worker([&] {
        for (int i = 0; i < 100; ++i) assert(engine.initializeWorld());
    });
    host.join();
    worker.join();
    assert(engine.world_ && engine.worldInitializationError_.empty());
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "readiness.cpp", directory / "readiness"
    unit.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    str(unit), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("PASS: actual startup entry logic — host order, failure/retry, stale error clearing, concurrent sample-rate changes")
