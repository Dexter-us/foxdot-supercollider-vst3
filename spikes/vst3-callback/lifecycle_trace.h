#pragma once

#include <chrono>
#include <cstdio>
#include <cstring>
#if defined(_WIN32)
#include <filesystem>
#include <fstream>
#include <mutex>
#include <process.h>
#endif

// Non-realtime lifecycle diagnostics only. Flush every record so a hung
// validator leaves the last entered phase in its redirected stderr log.
class LifecycleTrace {
public:
    explicit LifecycleTrace(const char* phase, const void* instance = nullptr)
        : phase_(phase), instance_(instance), start_(Clock::now()) {
        char record[512]{};
        std::snprintf(record, sizeof(record), "BEGIN %s instance=%p",
                      phase_, instance_);
        write(record);
    }
    ~LifecycleTrace() {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_).count();
        char record[512]{};
        std::snprintf(record, sizeof(record), "END %s instance=%p elapsed_ms=%lld",
                      phase_, instance_, static_cast<long long>(ms));
        write(record);
    }
    static void failure(const char* phase, unsigned long error) noexcept {
        char record[512]{};
        std::snprintf(record, sizeof(record), "FAIL %s Windows_error=%lu", phase, error);
        write(record);
    }
    LifecycleTrace(const LifecycleTrace&) = delete;
    LifecycleTrace& operator=(const LifecycleTrace&) = delete;
private:
    static void write(const char* record) noexcept {
        std::fprintf(stderr, "[Dexter lifecycle] %s\n", record);
        std::fflush(stderr);
#if defined(_WIN32)
        // Module termination may run under the loader lock or after static
        // logging state has been destroyed. Keep its original stderr trace only.
        if (std::strstr(record, "deinitialize_library")) return;
        // A GUI host may discard stderr. Preserve phase/timing records without
        // recording scripts or other user content. Logging must never fail a host.
        try {
            static std::mutex logMutex;
            std::lock_guard<std::mutex> lock(logMutex);
            std::error_code error;
            const auto folder = std::filesystem::temp_directory_path(error);
            if (error) return;
            const auto path = folder / ("Dexter-VST-lifecycle-" +
                std::to_string(_getpid()) + ".log");
            const auto size = std::filesystem::file_size(path, error);
            const auto mode = !error && size > 2 * 1024 * 1024
                ? std::ios::trunc : std::ios::app;
            std::ofstream output(path, mode);
            output << "[Dexter lifecycle] " << record << '\n';
        } catch (...) {
        }
#endif
    }
    using Clock = std::chrono::steady_clock;
    const char* phase_;
    const void* instance_;
    Clock::time_point start_;
};
