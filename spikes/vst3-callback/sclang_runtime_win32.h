#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "sclang_runtime_limits.h"

namespace dexter::sclang {

// Probes a loopback UDP port for the private host-driven World bridge.
// The caller binds it immediately with World_OpenTCP.
int findAvailableServerPort();

class Runtime {
public:
    Runtime() = default;
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    // Queues the latest source without blocking the editor or audio thread.
    void submit(const std::filesystem::path& runtimeRoot,
                int serverPort,
                int firstUserAudioBus,
                double sampleRate,
                std::string source);
    void stop();
    void shutdown();

    // Includes the most recent sclang output tail for the editor's Copy Error
    // action. The visible status bar uses the leading short status sentence.
    std::string status() const;

private:
    void workerLoop();
    bool ensureStarted(const std::filesystem::path& runtimeRoot,
                       int serverPort,
                       int firstUserAudioBus,
                       double sampleRate,
                       std::uint64_t cancellation);
    bool startProcess(const std::filesystem::path& runtimeRoot,
                      int serverPort,
                      int firstUserAudioBus,
                      double sampleRate,
                      std::uint64_t cancellation);
    bool sendSource(const std::string& source);
    bool sendControl(const char* address);
    void setStatus(std::string value);
    void releaseProcess(void* expected);

    mutable std::mutex mutex_;
    std::mutex lifecycleMutex_;
    std::condition_variable condition_;
    std::thread worker_;
    bool shutdownRequested_ = false;
    bool pendingRun_ = false;
    std::string pendingSource_;
    std::filesystem::path runtimeRoot_;
    std::filesystem::path workingDirectory_;
    std::filesystem::path logPath_;
    int serverPort_ = 0;
    int firstUserAudioBus_ = 2;
    double sampleRate_ = 44100.0;
    int languagePort_ = 0;
    std::uint64_t sourceGeneration_ = 0;
    std::uint64_t cancellationGeneration_ = 0;
    std::string status_ = "SuperCollider tab ready; sclang starts on first Run.";
#if defined(_WIN32)
    void* processHandle_ = nullptr;
    void* jobHandle_ = nullptr;
#endif
};

} // namespace dexter::sclang
