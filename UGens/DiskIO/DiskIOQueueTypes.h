// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace dexter_disk {
struct Barrier {
    std::mutex mutex;
    std::condition_variable ready;
    bool complete = false;
    std::atomic<unsigned> references{2}; // caller and queued request
    void release() {
        if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }
    void signal() {
        {
            std::lock_guard<std::mutex> guard(mutex);
            complete = true;
        }
        ready.notify_all();
        release();
    }
};

// File-format handling remains in libsndfile. These helpers only make short
// loops/read errors finite, and enforce the recording's explicit duration cap.
static void readLoop(SNDFILE* file, float* destination, sf_count_t frames, int channels) {
    sf_count_t done = 0;
    bool rewoundWithoutProgress = false;
    while (file && done < frames) {
        const auto count = sf_readf_float(file, destination + done * channels, frames - done);
        if (count > 0) {
            done += count;
            rewoundWithoutProgress = false;
        } else {
            if (rewoundWithoutProgress || sf_seek(file, 0, SEEK_SET) < 0) break;
            rewoundWithoutProgress = true;
        }
    }
    std::fill(destination + done * channels, destination + frames * channels, 0.0f);
}
static void writeFrames(World*, int, const float*, sf_count_t);
}
