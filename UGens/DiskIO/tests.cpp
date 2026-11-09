// SPDX-License-Identifier: GPL-2.0-or-later
// Calls the actual pinned DiskIO Unit constructors/calculation/destructors with
// a minimal SC InterfaceTable. No SC engine/server or VST SDK build is needed.
#include "DiskIOBridge.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "SC_PlugIn.h"
#include "../../spikes/vst3-callback/portable_diskio.h"
#include <sndfile.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;
static thread_local bool inAudio = false;
static std::atomic<unsigned> audioIO{0};
struct Definition { std::size_t size; UnitCtorFunc ctor; UnitDtorFunc dtor; };
static std::map<std::string, Definition> definitions;
static void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
using DefineUnitResult = decltype(std::declval<InterfaceTable&>().fDefineUnit(
    nullptr, std::size_t{}, UnitCtorFunc{}, UnitDtorFunc{}, uint32{}));
static DefineUnitResult defineUnit(const char* name, std::size_t size, UnitCtorFunc ctor,
                                   UnitDtorFunc dtor, uint32) {
    definitions.emplace(name, Definition{size, ctor, dtor}); return true;
}
static void clearUnit(Unit* unit, int32 frames) {
    for (unsigned channel = 0; channel < unit->mNumOutputs; ++channel)
        std::fill(unit->mOutBuf[channel], unit->mOutBuf[channel] + frames, 0.0f);
}
struct FakeWorld {
    World world{};
    std::array<SndBuf, 16> rt{}, nrt{};
    std::array<SndBufUpdates, 16> updates{};
    std::mutex lock;
    std::atomic<unsigned> completions{0};
    FakeWorld() {
        world.mNumSndBufs = rt.size();
        world.mSndBufs = rt.data();
        world.mSndBufsNonRealTimeMirror = nrt.data();
        world.mSndBufUpdates = updates.data();
        world.mNRTLock = this;
        world.mBufLength = 64;
        world.mSampleRate = 48000;
        world.mRealTime = false; // actual host-driven VST configuration
    }
    void await(unsigned count) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (completions.load() < count) {
            check(std::chrono::steady_clock::now() < until, "Disk worker failed to make progress");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
static void nrtLock(World* world) {
    if (inAudio) ++audioIO;
    static_cast<FakeWorld*>(world->mNRTLock)->lock.lock();
}
static void nrtUnlock(World* world) {
    auto& fake = *static_cast<FakeWorld*>(world->mNRTLock);
    ++fake.completions;
    fake.lock.unlock();
}
struct TestUnit {
    Definition definition;
    Unit* unit;
    Graph graph{};
    Rate rate{};
    std::vector<std::array<float, 64>> inputs, outputs;
    std::vector<float*> inPointers, outPointers;
    std::vector<Wire> wires;
    std::vector<Wire*> wirePointers;
    TestUnit(const std::string& name, FakeWorld& fake, unsigned nIn, unsigned nOut)
        : definition(definitions.at(name)),
          unit(static_cast<Unit*>(std::calloc(1, definition.size))),
          inputs(nIn), outputs(nOut), inPointers(nIn), outPointers(nOut),
          wires(nIn), wirePointers(nIn) {
        check(unit != nullptr, "Unit allocation failed");
        rate.mSampleRate = 48000;
        rate.mSampleDur = 1.0 / 48000;
        rate.mBufLength = 64;
        rate.mSlopeFactor = 1.0 / 64;
        // Per-unit mRate is set below; Graph has no full-rate pointer in SC 3.14.1.
        graph.localBufNum = -1;
        unit->mWorld = &fake.world;
        unit->mParent = &graph;
        unit->mRate = &rate;
        unit->mNumInputs = nIn;
        unit->mNumOutputs = nOut;
        unit->mBufLength = 64;
        for (unsigned i = 0; i < nIn; ++i) {
            inPointers[i] = inputs[i].data();
            wires[i].mCalcRate = calc_BufRate;
            wirePointers[i] = &wires[i];
        }
        for (unsigned i = 0; i < nOut; ++i) outPointers[i] = outputs[i].data();
        unit->mInput = wirePointers.data();
        unit->mInBuf = inPointers.data();
        unit->mOutBuf = outPointers.data();
    }
    void start() { definition.ctor(unit); }
    void run() { inAudio = true; unit->mCalcFunc(unit, 64); inAudio = false; }
    void stop() {
        if (!unit) return;
        if (definition.dtor) { inAudio = true; definition.dtor(unit); inAudio = false; }
        std::free(unit); unit = nullptr;
    }
    ~TestUnit() { stop(); }
};
static SNDFILE* openFile(const fs::path& path, int mode, SF_INFO& info) {
#ifdef _WIN32
    return sf_wchar_open(path.c_str(), mode, &info);
#else
    return sf_open(path.c_str(), mode, &info);
#endif
}
static void fixture(const fs::path& path, int frames, int channels) {
    SF_INFO info{};
    info.samplerate = 48000; info.channels = channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    auto* file = openFile(path, SFM_WRITE, info);
    check(file != nullptr, "Cannot create test fixture");
    std::vector<float> samples(frames * channels);
    for (int frame = 0; frame < frames; ++frame)
        for (int ch = 0; ch < channels; ++ch)
            samples[frame * channels + ch] =
                static_cast<float>(0.4 * std::sin(2 * 3.141592653589793 * frame / 160)) *
                (ch == 0 ? 1.0f : -0.5f);
    check(sf_writef_float(file, samples.data(), frames) == frames, "Fixture write failed");
    check(sf_close(file) == 0, "Fixture close failed");
}
static void readTest(const DexterDiskIOApi& api, const fs::path& root,
                     const char* name, int channels, float speed, bool shortLoop) {
    FakeWorld fake;
    char error[1024]{};
    DexterDiskBufferInfo info{};
    const auto relative = shortLoop ? "short.wav" : (channels == 1 ? "mono.wav" : "stereo.wav");
    check(api.openRead(&fake.world, 1, root.u8string().c_str(), relative, 1,
                       &info, error, sizeof(error)), error);
    check(info.channels == static_cast<unsigned>(channels), "Incorrect reader channel count");
    TestUnit unit(name, fake, std::strcmp(name, "DiskIn") == 0 ? 2 : 4, channels);
    unit.inputs[0][0] = 1;
    if (std::strcmp(name, "DiskIn") == 0) unit.inputs[1][0] = 1;
    else { unit.inputs[1][0] = speed; unit.inputs[2][0] = 1; }
    unit.start();
    double energy = 0, errorEnergy = 0, position = 0;
    unsigned expectedRequests = 0;
    const int sourceLength = shortLoop ? 3200 : 96000;
    for (int block = 0; block < 2400; ++block) {
        const bool interpolated = std::strcmp(name, "VDiskIn") == 0;
        unit.run();
        for (int frame = 0; frame < 64; ++frame) {
            // Upstream VDiskIn emits sample zero separately on its first
            // calculation and advances the interpolator from output sample 1.
            const auto inputPosition = std::fmod(std::max(0.0,
                position + frame * speed - (interpolated ? speed : 0.0)), sourceLength);
            for (int ch = 0; ch < channels; ++ch) {
                const double expected = 0.4 * std::sin(2 * 3.141592653589793 * inputPosition / 160) *
                                        (ch == 0 ? 1.0 : -0.5);
                const auto actual = unit.outputs[ch][frame];
                energy += actual * actual;
                errorEnergy += (actual - expected) * (actual - expected);
            }
        }
        position += 64 * speed;
        const auto needed = static_cast<unsigned>(std::max(0.0,
            interpolated ? (position - speed - 1) / 32768 : position / 32768));
        if (needed > expectedRequests) {
            expectedRequests = needed;
            fake.await(expectedRequests);
        }
    }
    check(energy > 1, "Reader output is silent");
    check(errorEnergy / energy < 0.00001, std::string(name) + " sample/rate mismatch");
    unit.stop();
    check(api.closeBuffer(&fake.world, 1, 3000, error, sizeof(error)), error);
    check(!fake.rt[1].data && !fake.nrt[1].sndfile, "Reader buffer was not detached");
    std::cout << "PASS " << name << " channels=" << channels << " rate=" << speed
              << (shortLoop ? " short-loop" : "") << std::endl;
}
static fs::path recordTest(const DexterDiskIOApi& api, const fs::path& root, double limit) {
    FakeWorld fake;
    char path[4096]{}, error[1024]{};
    check(api.openWrite(&fake.world, 2, root.u8string().c_str(), 2, 48000,
                        limit, path, sizeof(path), error, sizeof(error)), error);
    TestUnit unit("DiskOut", fake, 3, 1);
    unit.inputs[0][0] = 2;
    unit.start();
    constexpr int frames = 150016; // includes ring refills + final partial chunk
    unsigned requests = 0;
    for (int start = 0; start < frames; start += 64) {
        for (int i = 0; i < 64; ++i) {
            unit.inputs[1][i] = static_cast<float>(0.25 * std::sin((start + i) * .031));
            unit.inputs[2][i] = -unit.inputs[1][i];
        }
        unit.run();
        if ((start + 64) % 32768 == 0) fake.await(++requests);
    }
    unit.stop();
    check(api.closeBuffer(&fake.world, 2, 3000, error, sizeof(error)), error);
    SF_INFO info{};
    auto* file = openFile(fs::u8path(path), SFM_READ, info);
    check(file != nullptr, "Finalized recording cannot be reopened");
    const auto expectedFrames = std::min<sf_count_t>(frames, std::ceil(limit * 48000));
    check(info.frames == expectedFrames && info.channels == 2,
          "Recording duration/channel/header mismatch");
    std::vector<float> samples(info.frames * info.channels);
    check(sf_readf_float(file, samples.data(), info.frames) == info.frames, "Recording readback failed");
    sf_close(file);
    for (sf_count_t i = 0; i < info.frames; ++i) {
        const auto expected = static_cast<float>(0.25 * std::sin(i * .031));
        check(std::abs(samples[i * 2] - expected) < 1e-6 &&
              std::abs(samples[i * 2 + 1] + expected) < 1e-6,
              "DiskOut samples differ / final partial buffer lost");
    }
    check(!fake.rt[2].data && !fake.nrt[2].sndfile, "Recording buffer was not detached");
    std::cout << "PASS DiskOut finalized " << info.frames << " stereo frames" << std::endl;
    return fs::u8path(path);
}
static void controllerTest(const fs::path& portable) {
    FakeWorld fake;
    std::mutex worldMutex;
    PortableDiskIO controller;
    controller.preload(portable);
    controller.startMaintenance(worldMutex, [](const std::string& error) {
        std::cerr << error << std::endl;
    });
    std::string status, error;
    PythonDiskCommand read;
    read.mode = 1; read.path = "mono.wav"; read.loop = true;
    {
        std::lock_guard<std::mutex> lock(worldMutex);
        check(controller.execute(&fake.world, portable, {read}, status, error), error);
        double energy = 0;
        for (int block = 0; block < 32; ++block) {
            inAudio = true; controller.renderBlock(); inAudio = false;
            for (int i = 0; i < 64; ++i)
                energy += controller.left(i) * controller.left(i);
        }
        check(energy > 1, "VST disk controller playback is silent");
        PythonDiskCommand stop; stop.mode = 0;
        check(controller.execute(&fake.world, portable, {stop}, status, error), error);
        PythonDiskCommand record; record.mode = 3; record.seconds = .1;
        check(controller.execute(&fake.world, portable, {record}, status, error), error);
        const auto path = fs::u8path(status.substr(status.rfind(": ") + 2));
        for (int i = 0; i < 5000; ++i) {
            inAudio = true; controller.captureSample(.2f, -.1f); inAudio = false;
        }
        stop.mode = 4;
        check(controller.execute(&fake.world, portable, {stop}, status, error), error);
        SF_INFO info{};
        auto* file = openFile(path, SFM_READ, info);
        check(file && info.frames == 4800 && info.channels == 2,
              "VST controller auto-stop recording/header failed");
        std::vector<float> recorded(info.frames * 2);
        check(sf_readf_float(file, recorded.data(), info.frames) == info.frames,
              "VST controller recording readback failed");
        sf_close(file);
        for (int i = 0; i < 4800; ++i)
            check(recorded[i * 2] == .2f && recorded[i * 2 + 1] == -.1f,
                  "VST controller recorded incorrect master samples");
        record.seconds = 10;
        check(controller.execute(&fake.world, portable, {record}, status, error), error);
        const auto earlyPath = fs::u8path(status.substr(status.rfind(": ") + 2));
        for (int i = 0; i < 12345; ++i) {
            inAudio = true; controller.captureSample(.25f, -.125f); inAudio = false;
        }
        check(controller.execute(&fake.world, portable, {stop}, status, error), error);
        info = {};
        file = openFile(earlyPath, SFM_READ, info);
        check(file && info.frames == 12345, "Manual stop lost/padded the last partial audio block");
        sf_close(file);
        check(controller.execute(&fake.world, portable, {read}, status, error), error);
    }
    controller.requestStop(); // same nonblocking path as the VST Stop button
    for (int attempt = 0; attempt < 200; ++attempt) {
        bool detached = true;
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            for (const auto& buffer : fake.rt) detached &= buffer.data == nullptr;
        }
        if (detached) break;
        check(attempt != 199, "VST controller Stop did not release disk buffers");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    controller.shutdown();
    std::cout << "PASS actual VST disk controller playback, master capture, auto-finalize and asynchronous Stop\n";
}
int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: diskio-tests module.scx scratch-directory");
        const auto root = fs::absolute(fs::u8path(argv[2])) / "Portable";
        fs::create_directories(root / "SCPlugins");
        const auto modulePath = root / "SCPlugins" / "DiskIO_UGens.scx";
        fs::copy_file(fs::u8path(argv[1]), modulePath, fs::copy_options::overwrite_existing);
#ifdef _WIN32
        const auto dependency = fs::u8path(argv[1]).parent_path() / "sndfile.dll";
        fs::copy_file(dependency, modulePath.parent_path() / "sndfile.dll",
                      fs::copy_options::overwrite_existing);
#endif
#ifdef _WIN32
        const auto module = LoadLibraryExW(modulePath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        check(module != nullptr, "Cannot load DiskIO module");
        auto symbol = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(module, name)); };
#else
        const auto module = dlopen(modulePath.c_str(), RTLD_NOW | RTLD_LOCAL);
        check(module != nullptr, "Cannot load DiskIO module");
        auto symbol = [&](const char* name) { return dlsym(module, name); };
#endif
        const auto loadModule = reinterpret_cast<LoadPlugInFunc>(symbol("load"));
        const auto unloadModule = reinterpret_cast<UnLoadPlugInFunc>(symbol("unload"));
        const auto getApi = reinterpret_cast<DexterDiskIOGetApi>(symbol("DexterDiskIO_GetApi"));
        check(loadModule && unloadModule && getApi, "Required module exports missing");
        InterfaceTable table{};
        table.fDefineUnit = defineUnit;
        table.fClearUnitOutputs = clearUnit;
        table.fNRTLock = nrtLock; table.fNRTUnlock = nrtUnlock;
        table.fSendNodeReply = [](Node*, int32, const char*, int, const float*) {};
        loadModule(&table);
        const auto& api = *getApi();
        check(api.version == 1 && api.scApiVersion == sc_api_version && definitions.size() == 3,
              "Bridge/SC ABI mismatch or missing DiskIO UGen");
        fs::create_directories(root / "Samples");
        fixture(root / "Samples" / "mono.wav", 96000, 1);
        fixture(root / "Samples" / "stereo.wav", 96000, 2);
        fixture(root / "Samples" / "short.wav", 3200, 1);
        readTest(api, root / "Samples", "DiskIn", 1, 1, false);
        readTest(api, root / "Samples", "DiskIn", 2, 1, false);
        readTest(api, root / "Samples", "DiskIn", 1, 1, true);
        readTest(api, root / "Samples", "VDiskIn", 1, .5f, false);
        readTest(api, root / "Samples", "VDiskIn", 2, 2.0f, false);
        const auto a = recordTest(api, root / "Recordings", 10);
        const auto b = recordTest(api, root / "Recordings", .1);
        check(a != b && fs::exists(a), "Recording filename reused or overwritten");
        // Two independent VST Worlds must be able to enqueue concurrently.
        std::exception_ptr failures[2];
        std::thread producers[2];
        for (int i = 0; i < 2; ++i)
            producers[i] = std::thread([&, i] {
                try { recordTest(api, root / "Recordings", 10); }
                catch (...) { failures[i] = std::current_exception(); }
            });
        for (auto& thread : producers) thread.join();
        for (const auto& failure : failures) if (failure) std::rethrow_exception(failure);
        controllerTest(root);
        FakeWorld fake;
        DexterDiskBufferInfo info{}; char error[1024]{};
        check(!api.openRead(&fake.world, 1, (root / "Samples").u8string().c_str(),
                            "../Recordings/not-allowed.wav", 0, &info, error, sizeof(error)),
              "Escaping sample path was accepted");
        check(!api.openWrite(&fake.world, 1, (root / "Recordings").u8string().c_str(),
                             2, 48000, INFINITY, nullptr, 0, error, sizeof(error)),
              "Unbounded recording was accepted");
        check(!api.openWrite(&fake.world, 1, (root / "Recordings").u8string().c_str(),
                             2, 384000, 3600, nullptr, 0, error, sizeof(error)),
              "Oversized WAV exceeded the 2 GiB recording limit");
        std::error_code linkError;
        fs::create_directory_symlink(root / "Recordings", root / "Samples" / "escape", linkError);
        if (!linkError)
            check(!api.openRead(&fake.world, 1, (root / "Samples").u8string().c_str(),
                                ("escape/" + a.filename().u8string()).c_str(), 0,
                                &info, error, sizeof(error)), "Symlink sample escape accepted");
        check(audioIO == 0, "Synchronous disk work ran on the simulated audio callback");
        check(api.droppedRequests() == 0, "Disk request queue overflowed");
        unloadModule();
#ifdef _WIN32
        FreeLibrary(module);
#else
        dlclose(module);
#endif
        std::cout << "PASS no audio-thread disk I/O; safe paths; unique names; two concurrent Worlds\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << std::endl;
        return 1;
    }
}
