#pragma once

#include <chrono>
#include <cstdio>

// Non-realtime lifecycle diagnostics only. Flush every record so a hung
// validator leaves the last entered phase in its redirected stderr log.
class LifecycleTrace {
public:
    explicit LifecycleTrace(const char* phase, const void* instance = nullptr)
        : phase_(phase), instance_(instance), start_(Clock::now()) {
        std::fprintf(stderr, "[Dexter lifecycle] BEGIN %s instance=%p\n",
                     phase_, instance_);
        std::fflush(stderr);
    }
    ~LifecycleTrace() {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_).count();
        std::fprintf(stderr, "[Dexter lifecycle] END %s instance=%p elapsed_ms=%lld\n",
                     phase_, instance_, static_cast<long long>(ms));
        std::fflush(stderr);
    }
    LifecycleTrace(const LifecycleTrace&) = delete;
    LifecycleTrace& operator=(const LifecycleTrace&) = delete;
private:
    using Clock = std::chrono::steady_clock;
    const char* phase_;
    const void* instance_;
    Clock::time_point start_;
};
