#!/usr/bin/env python3
"""Compile the actual SC submit/stop/status methods against a gated fake engine.

This tests off-UI queueing and cancellation, not real sclang audio or VSTHost.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "spikes/vst3-callback/processor.cpp").read_text()
methods = source[source.index("    void submitSuperColliderCode("):
                 source.index("    float masterOutputGain()")]
code = r"""
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
using namespace std::chrono_literals;
struct Host {
    virtual void submitSuperColliderCode(const std::string&) = 0;
    virtual void stopSuperCollider() = 0;
    virtual std::string superColliderStatus() = 0;
};
struct FakeRuntime {
    std::atomic<int> submits{0};
    std::string source;
    bool throwOnSubmit = false;
    void submit(std::filesystem::path, int port, int bus, double rate, std::string text) {
        assert(port == 58000 && bus == 256 && rate == 48000);
        if (throwOnSubmit) throw std::runtime_error("injected submit failure");
        source = text;
        ++submits;
    }
    void stop() {}
    std::string status() { return "Language worker queued"; }
};
struct Engine : Host {
    static constexpr unsigned kMaxEditorSourceBytes = 256 * 1024;
    static constexpr int kSclangFirstAllocatableAudioBus = 256;
    std::mutex scriptThreadMutex_, worldAccessMutex_, statusMutex_, gateMutex_;
    std::condition_variable gate_;
    std::thread scriptThread_;
    std::atomic<bool> scriptRunning_{false}, scPreparationStatusOverride_{false};
    std::atomic<std::uint64_t> scSourceGeneration_{0};
    bool opened = false, fails = false;
    void* world_ = this;
    int scLanguageServerPort_ = 58000;
    double sampleRate_ = 48000;
    std::string message;
    FakeRuntime scLanguageRuntime_;
    ~Engine() { release(); finish(); }
    void release() { { std::lock_guard<std::mutex> lock(gateMutex_); opened = true; } gate_.notify_all(); }
    void finish() { if(scriptThread_.joinable()) scriptThread_.join(); }
    bool initializeWorld() {
        std::unique_lock<std::mutex> lock(gateMutex_);
        if (!gate_.wait_for(lock, 2s, [&] { return opened; }))
            throw std::runtime_error("test initialization gate timed out");
        if (fails) { setEditorStatus("Specific engine failure"); return false; }
        return true;
    }
    void setEditorStatus(std::string value) {
        std::lock_guard<std::mutex> lock(statusMutex_); message = value;
    }
    std::string editorStatus() {
        std::lock_guard<std::mutex> lock(statusMutex_); return message;
    }
    std::filesystem::path bundledSuperColliderLanguagePath() { return "runtime"; }
"""
code += methods
code += r"""
};
int main() {
    Engine e;
    auto start = std::chrono::steady_clock::now();
    e.submitSuperColliderCode("first");
    assert(std::chrono::steady_clock::now() - start < 100ms);
    assert(e.scriptRunning_ && e.scLanguageRuntime_.submits == 0);
    assert(e.superColliderStatus().find("worker") != std::string::npos);
    e.submitSuperColliderCode("busy");
    assert(e.editorStatus().find("previous") != std::string::npos);
    e.stopSuperCollider();
    e.release(); e.finish();
    assert(!e.scriptRunning_ && e.scLanguageRuntime_.submits == 0);
    e.fails = true;
    e.submitSuperColliderCode("bad engine"); e.finish();
    assert(!e.scriptRunning_ && e.superColliderStatus() == "Specific engine failure");
    e.fails = false;
    e.scLanguageServerPort_ = 0;
    e.submitSuperColliderCode("missing endpoint"); e.finish();
    assert(e.superColliderStatus().find("endpoint") != std::string::npos);
    e.scLanguageServerPort_ = 58000;
    e.scLanguageRuntime_.throwOnSubmit = true;
    e.submitSuperColliderCode("throw"); e.finish();
    assert(!e.scriptRunning_ && e.superColliderStatus().find("injected") != std::string::npos);
    e.scLanguageRuntime_.throwOnSubmit = false;
    e.submitSuperColliderCode("valid score"); e.finish();
    assert(!e.scriptRunning_ && e.scLanguageRuntime_.submits == 1);
    assert(e.scLanguageRuntime_.source == "valid score");
    assert(e.superColliderStatus() == "Language worker queued");
    e.submitSuperColliderCode(std::string(256 * 1024 + 1, 'x'));
    assert(e.editorStatus().find("invalid") != std::string::npos);
    e.submitSuperColliderCode(std::string("null\0byte", 9));
    assert(e.editorStatus().find("invalid") != std::string::npos);
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    unit, binary = directory / "submit.cpp", directory / "submit"
    unit.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-pthread", str(unit), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("PASS: actual SC submit returns before gated engine startup; busy, Stop, errors, retry, bounds and status")
