// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../../UGens/DiskIO/DiskIOBridge.h"
#include "python_runtime.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// All lifecycle methods are called with the processor's World mutex on a
// non-audio thread. Only renderBlock/captureSample/requestStop run in process().
class PortableDiskIO {
public:
    ~PortableDiskIO() {
        shutdown();
    }
    void shutdown() {
        exiting_.store(true);
        if (maintenance_.joinable()) maintenance_.join();
    }
    void preload(const std::filesystem::path& portable) {
        if (api_) return;
        const auto file = portable / "SCPlugins" / "DiskIO_UGens.scx";
        if (!std::filesystem::is_regular_file(file)) {
            moduleError_ = "Install the separate matching DiskIO pack in Portable/SCPlugins, then fully restart the DAW.";
            return;
        }
        // SC retains UGen registrations process-wide. Keep this module reference
        // for the same lifetime; never unload code while another World uses it.
#ifdef _WIN32
        auto module = LoadLibraryExW(file.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        const auto getApi = module ? reinterpret_cast<DexterDiskIOGetApi>(
            GetProcAddress(module, "DexterDiskIO_GetApi")) : nullptr;
#else
        auto module = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
        const auto getApi = module ? reinterpret_cast<DexterDiskIOGetApi>(
            dlsym(module, "DexterDiskIO_GetApi")) : nullptr;
#endif
        if (!getApi) {
            moduleError_ = "DiskIO pack is missing, incompatible, or cannot find its adjacent libsndfile library. Restore the complete matching pack and restart the DAW.";
            return;
        }
        api_ = getApi();
        if (api_->version != 1 || api_->scApiVersion != 7) {
            api_ = nullptr; moduleError_ = "DiskIO bridge/SuperCollider API versions do not match this VST.";
        }
    }
    void startMaintenance(std::mutex& worldMutex,
                          std::function<void(const std::string&)> status) {
        if (maintenance_.joinable()) return;
        exiting_.store(false);
        maintenance_ = std::thread([this, &worldMutex, status = std::move(status)] {
            while (!exiting_.load()) {
                if (stopRequested_.load()) {
                    std::string error;
                    {
                        std::lock_guard<std::mutex> lock(worldMutex);
                        if (closeAll(error)) stopRequested_.store(false);
                    }
                    if (!error.empty()) status("DiskIO stop failed: " + error);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }
    bool execute(World* world, const std::filesystem::path& portable,
                 const std::vector<PythonDiskCommand>& commands,
                 std::string& status, std::string& error) {
        if (!api_) { error = moduleError_; return false; }
        if (stopRequested_.load()) {
            if (!closeAll(error)) return false;
            stopRequested_.store(false);
        }
        world_ = world;
        for (const auto& command : commands) {
            if (command.mode == 0) {
                if (!close(readers_[command.slot], error) || !error.empty()) return false;
                status = "Disk stream stopped.";
            } else if (command.mode == 4) {
                if (!close(recorder_, error) || !error.empty()) return false;
                status = "Recording stopped and WAV finalized.";
            } else if (command.mode == 3) {
                DexterDiskBufferInfo previous{};
                if (recorder_.buffer && api_->inspectBuffer(world_, recorder_.buffer, &previous) &&
                    (previous.ioError || previous.writtenFrames == previous.limitFrames) &&
                    (!close(recorder_, error) || !error.empty())) return false;
                if (recorder_.unit) {
                    error = "A disk recording is already open. Run DiskOut.stop() before starting another.";
                    return false;
                }
                const auto id = freeBuffer();
                if (!id) { error = "No free SuperCollider buffer is available for recording."; return false; }
                char message[1024]{}, createdPath[4096]{};
                if (!api_->openWrite(world_, id, (portable / "Recordings").u8string().c_str(),
                                     2, static_cast<unsigned>(std::lround(world_->mSampleRate)),
                                     command.seconds, createdPath, sizeof(createdPath),
                                     message, sizeof(message))) {
                    error = message; return false;
                }
                recorder_.buffer = id;
                recorder_.unit = api_->createUnit(world_, id, 3, 1, message, sizeof(message));
                if (!recorder_.unit) {
                    error = message; std::string cleanup; close(recorder_, cleanup); return false;
                }
                status = "Recording post-master stereo output for up to " +
                         std::to_string(command.seconds) + " seconds: " + createdPath;
            } else {
                auto& reader = readers_[command.slot];
                if (!close(reader, error) || !error.empty()) return false;
                const auto id = freeBuffer();
                if (!id) { error = "No free SuperCollider buffer is available for disk playback."; return false; }
                char message[1024]{};
                DexterDiskBufferInfo info{};
                if (!api_->openRead(world_, id, (portable / "Samples").u8string().c_str(),
                                    command.path.c_str(), command.loop, &info, message, sizeof(message))) {
                    error = message; return false;
                }
                reader.buffer = id;
                const auto rate = static_cast<float>(command.rate * info.sampleRate / world_->mSampleRate);
                reader.unit = api_->createUnit(world_, id, command.mode, rate, message, sizeof(message));
                if (!reader.unit) {
                    error = message; std::string cleanup; close(reader, cleanup); return false;
                }
                reader.amplitude = static_cast<float>(command.amp);
                reader.pan = static_cast<float>(command.pan);
                reader.gain = 0;
                status = std::string(command.mode == 1 ? "DiskIn" : "VDiskIn") +
                         " streaming " + command.path + " in slot " + std::to_string(command.slot + 1) + ".";
                if (command.mode == 1 && std::abs(info.sampleRate - world_->mSampleRate) > .01)
                    status += " Source/host rates differ: DiskIn plays at host rate, as in SC; use VDiskIn for rate compensation.";
            }
        }
        return true;
    }
    void renderBlock() {
        left_.fill(0); right_.fill(0);
        if (!api_ || stopRequested_.load(std::memory_order_relaxed)) return;
        for (auto& reader : readers_) {
            if (!reader.unit) continue;
            std::array<float, 64> left{}, right{};
            api_->renderReader(reader.unit, left.data(), right.data(), 64);
            const auto leftBalance = std::sqrt(std::max(0.0f, 1.0f - reader.pan));
            const auto rightBalance = std::sqrt(std::max(0.0f, 1.0f + reader.pan));
            const float step = static_cast<float>(1.0 - std::exp(-1.0 / (0.01 * world_->mSampleRate)));
            for (unsigned i = 0; i < 64; ++i) {
                reader.gain += step * (reader.amplitude - reader.gain);
                left_[i] += left[i] * reader.gain * leftBalance;
                right_[i] += right[i] * reader.gain * rightBalance;
            }
        }
    }
    float left(unsigned i) const { return left_[i]; }
    float right(unsigned i) const { return right_[i]; }
    void captureSample(float left, float right) {
        if (api_ && recorder_.unit && !stopRequested_.load(std::memory_order_relaxed))
            api_->captureSample(recorder_.unit, left, right);
    }
    void requestStop() { stopRequested_.store(true); }
    bool closeAll(std::string& error) {
        if (world_ && api_) {
            // One timed-out global FIFO is enough; do not repeat its five-second
            // wait for every remaining slot during a failing teardown.
            for (auto& reader : readers_) if (!close(reader, error)) return false;
            if (!close(recorder_, error)) return false;
        }
        world_ = nullptr;
        return true;
    }
private:
    struct Slot {
        void* unit = nullptr;
        unsigned buffer = 0;
        float amplitude = 1, pan = 0, gain = 0;
    };
    bool close(Slot& slot, std::string& error) {
        if (!slot.buffer) return true;
        if (slot.unit) { api_->destroyUnit(slot.unit); slot.unit = nullptr; }
        char message[1024]{};
        if (!api_->closeBuffer(world_, slot.buffer, 5000, message, sizeof(message))) {
            // Do not free a buffer/World while queued I/O can still reference it.
            error = message;
            DexterDiskBufferInfo remaining{};
            if (!api_->inspectBuffer(world_, slot.buffer, &remaining)) {
                slot = {}; // finalized with an I/O error, but no pending ownership
                return true;
            }
            return false;
        }
        slot = {}; return true;
    }
    unsigned freeBuffer() const {
        for (unsigned i = world_->mNumSndBufs; i > 1; --i)
            if (!world_->mSndBufs[i - 1].data && !world_->mSndBufs[i - 1].sndfile)
                return i - 1;
        return 0;
    }
    const DexterDiskIOApi* api_ = nullptr;
    World* world_ = nullptr;
    std::string moduleError_ = "DiskIO pack is not loaded.";
    std::array<Slot, 8> readers_{};
    Slot recorder_;
    std::array<float, 64> left_{}, right_{};
    std::atomic<bool> stopRequested_{false}, exiting_{false};
    std::thread maintenance_;
};
