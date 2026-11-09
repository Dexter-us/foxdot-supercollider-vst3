// SPDX-License-Identifier: GPL-2.0-or-later
// Compiled in the same translation unit as upstream DiskIO_UGens.cpp.
#include "DiskIOBridge.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace dexter_disk {
namespace fs = std::filesystem;
struct BufferOwner {
    std::vector<float> ring;
    SNDFILE* file = nullptr;
    DexterDiskBufferInfo info{};
    bool recording = false;
    bool looping = false;
    std::string path;
    ~BufferOwner() { if (file) sf_close(file); }
};
static std::mutex ownersMutex;
static std::map<std::pair<World*, unsigned>, std::unique_ptr<BufferOwner>> owners;
static std::atomic<std::uint64_t> nextRecording{0};
constexpr int ringFrames = 65536;

static void copyText(char* out, std::size_t size, const std::string& text) {
    if (!out || size == 0) return;
    const auto count = std::min(size - 1, text.size());
    std::memcpy(out, text.data(), count);
    out[count] = '\0';
}
static int failure(char* out, std::size_t size, const std::string& text) {
    copyText(out, size, text); return 0;
}
static bool emptySlot(World* world, unsigned id) {
    return world && id > 0 && id < world->mNumSndBufs &&
           !world->mSndBufs[id].data && !world->mSndBufs[id].sndfile &&
           !world->mSndBufsNonRealTimeMirror[id].data;
}
static bool within(const fs::path& root, const fs::path& child) {
    auto a = root.begin(), b = child.begin();
    for (; a != root.end(); ++a, ++b)
        if (b == child.end() || *a != *b) return false;
    return b != child.end();
}
static SNDFILE* openReadFile(const fs::path& path, SF_INFO& info) {
#ifdef _WIN32
    return sf_wchar_open(path.c_str(), SFM_READ, &info);
#else
    return sf_open(path.c_str(), SFM_READ, &info);
#endif
}
static void publish(World* world, unsigned id, BufferOwner& owner, SF_INFO& info, bool loop) {
    SndBuf buffer{};
    buffer.channels = info.channels;
    buffer.frames = ringFrames;
    buffer.samples = ringFrames * info.channels;
    buffer.samplerate = info.samplerate;
    buffer.sampledur = 1.0 / info.samplerate;
    buffer.data = owner.ring.data();
    buffer.sndfile = owner.file;
    buffer.mask = buffer.mask1 = -1;
    if (!owner.recording && !loop && info.frames < ringFrames)
        buffer.mask = buffer.mask1 = static_cast<int>(info.frames);
    world->mSndBufs[id] = world->mSndBufsNonRealTimeMirror[id] = buffer;
    ++world->mSndBufUpdates[id].writes;
}
static int openRead(World* world, unsigned id, const char* rootText, const char* relativeText,
                    int loop, DexterDiskBufferInfo* output, char* error, std::size_t size) {
    try {
        if (!gDiskIO)
            return failure(error, size, "DiskIO UGens are not registered. Fully restart the DAW after installing the pack.");
        if (!rootText || !relativeText || !output || !emptySlot(world, id))
            return failure(error, size, "Disk reader needs an unused nonzero SC buffer.");
        const auto relative = fs::u8path(relativeText);
        if (relative.empty() || relative.is_absolute() || relative.has_root_name())
            return failure(error, size, "Disk input must be relative to Portable/Samples.");
        const auto root = fs::canonical(fs::u8path(rootText));
        const auto path = fs::canonical(root / relative);
        if (!within(root, path) || !fs::is_regular_file(path))
            return failure(error, size, "Disk input escapes Portable/Samples or is not a file.");
        auto owner = std::make_unique<BufferOwner>();
        SF_INFO info{};
        owner->file = openReadFile(path, info);
        if (!owner->file) return failure(error, size, "libsndfile could not open the disk input.");
        auto reject = [&](const char* message) {
            sf_close(owner->file); owner->file = nullptr;
            return failure(error, size, message);
        };
        if (info.frames <= 0 || info.channels < 1 || info.channels > 2 ||
            info.samplerate < 8000 || info.samplerate > 384000)
            return reject("Disk input must have audio frames, 1 or 2 channels, and an 8–384 kHz sample rate.");
        try { owner->ring.resize(ringFrames * info.channels); }
        catch (...) { return reject("Could not allocate the disk input ring buffer."); }
        if (loop) readLoop(owner->file, owner->ring.data(), ringFrames, info.channels);
        else {
            const auto count = sf_readf_float(owner->file, owner->ring.data(), ringFrames);
            if (count < 0) return reject("Disk input failed while cueing its first buffer.");
            std::fill(owner->ring.begin() + count * info.channels, owner->ring.end(), 0.0f);
        }
        owner->info = {static_cast<unsigned>(info.channels), static_cast<unsigned>(info.samplerate),
                       static_cast<std::uint64_t>(info.frames), 0, 0, 0};
        owner->path = path.u8string();
        owner->looping = loop != 0;
        std::lock_guard<std::mutex> guard(ownersMutex);
        if (!emptySlot(world, id) || owners.count({world, id}))
            return reject("The disk buffer slot was claimed while cueing.");
        *output = owner->info;
        auto* ready = owner.get();
        // Establish ownership before exposing either SC mirror to the allocation.
        owners.emplace(std::make_pair(world, id), std::move(owner));
        publish(world, id, *ready, info, loop != 0);
        return 1;
    } catch (const std::exception& ex) { return failure(error, size, ex.what()); }
}
static int openWrite(World* world, unsigned id, const char* rootText, unsigned channels,
                     unsigned sampleRate, double maxSeconds, char* pathOut, std::size_t pathSize,
                     char* error, std::size_t size) {
    try {
        if (!rootText || !emptySlot(world, id) || channels < 1 || channels > 2 ||
            sampleRate < 8000 || sampleRate > 384000 || !std::isfinite(maxSeconds) ||
            maxSeconds <= 0 || maxSeconds > 3600)
            return failure(error, size, "Recording needs a free SC buffer, 1–2 channels, and a duration in (0, 3600] seconds.");
        if (!gDiskIO)
            return failure(error, size, "DiskIO UGens are not registered. Restart the DAW.");
        if (maxSeconds * sampleRate * channels * sizeof(float) > 2147483648.0)
            return failure(error, size, "Recording exceeds the 2 GiB WAV safety limit. Choose a shorter duration.");
        const auto requested = fs::absolute(fs::u8path(rootText));
        if (fs::is_symlink(fs::symlink_status(requested)))
            return failure(error, size, "Portable/Recordings must not be a symlink.");
        fs::create_directories(requested);
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(requested.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return failure(error, size, "Portable/Recordings must be a normal local directory, not a junction/reparse point.");
#endif
        const auto root = fs::canonical(requested);
        auto owner = std::make_unique<BufferOwner>();
        owner->ring.resize(ringFrames * channels);
        int fd = -1;
        fs::path path;
        for (unsigned attempt = 0; attempt < 100 && fd < 0; ++attempt) {
            const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            path = root / ("recording-" + std::to_string(stamp) + "-" +
                           std::to_string(nextRecording.fetch_add(1)) + ".wav");
#ifdef _WIN32
            fd = _wopen(path.c_str(), _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                        _S_IREAD | _S_IWRITE);
#else
            fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
#endif
        }
        if (fd < 0) return failure(error, size, "Could not exclusively create a new recording WAV.");
        SF_INFO info{};
        info.channels = static_cast<int>(channels);
        info.samplerate = static_cast<int>(sampleRate);
        info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        owner->file = sf_open_fd(fd, SFM_WRITE, &info, SF_TRUE);
        if (!owner->file) {
#ifdef _WIN32
            _close(fd);
#else
            ::close(fd);
#endif
            fs::remove(path);
            return failure(error, size, "libsndfile could not initialize the new recording.");
        }
        owner->recording = true;
        owner->path = path.u8string();
        owner->info = {channels, sampleRate, 0, 0,
                       static_cast<std::uint64_t>(std::ceil(maxSeconds * sampleRate)), 0};
        std::lock_guard<std::mutex> guard(ownersMutex);
        if (!emptySlot(world, id) || owners.count({world, id})) {
            sf_close(owner->file); owner->file = nullptr; fs::remove(path);
            return failure(error, size, "The recording buffer slot was claimed while opening.");
        }
        copyText(pathOut, pathSize, owner->path);
        auto* ready = owner.get();
        owners.emplace(std::make_pair(world, id), std::move(owner));
        publish(world, id, *ready, info, false);
        return 1;
    } catch (const std::exception& ex) { return failure(error, size, ex.what()); }
}
static void writeFrames(World* world, int id, const float* source, sf_count_t frames) {
    std::lock_guard<std::mutex> guard(ownersMutex); // disk worker, never audio thread
    const auto found = owners.find({world, static_cast<unsigned>(id)});
    if (found == owners.end()) {
        // Retain standard DiskOut behavior for buffers managed by an SC client.
        auto* buffer = World_GetNRTBuf(world, id);
        if (buffer->sndfile) sf_writef_float(GETSNDFILE(buffer), source, frames);
        return;
    }
    auto& owner = *found->second;
    if (!owner.recording || !owner.file) return;
    const auto remaining = owner.info.limitFrames - owner.info.writtenFrames;
    const auto requested = static_cast<sf_count_t>(std::min<std::uint64_t>(remaining, frames));
    const auto count = sf_writef_float(owner.file, source, requested);
    if (count > 0) owner.info.writtenFrames += static_cast<std::uint64_t>(count);
    if (count != requested) owner.info.ioError = 1;
    if (owner.info.writtenFrames == owner.info.limitFrames || owner.info.ioError) {
        if (sf_close(owner.file) != 0) owner.info.ioError = 1;
        owner.file = nullptr;
        world->mSndBufs[id].sndfile = world->mSndBufsNonRealTimeMirror[id].sndfile = nullptr;
    }
}
static bool drain(unsigned timeoutMs) {
    if (!gDiskIO) return false;
    auto* barrier = new Barrier;
    const auto end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(timeoutMs);
    DiskIOMsg request{};
    request.mBarrier = barrier;
    while (!gDiskIO->Write(request)) {
        if (std::chrono::steady_clock::now() >= end) {
            barrier->release(); barrier->release(); return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    bool completed;
    {
        std::unique_lock<std::mutex> lock(barrier->mutex);
        completed = barrier->ready.wait_until(lock, end, [&] { return barrier->complete; });
    }
    barrier->release(); // on timeout the queued request keeps its own reference
    return completed;
}
static int closeBuffer(World* world, unsigned id, unsigned timeoutMs, char* error, std::size_t size) {
    // The caller has already stopped all nodes using this buffer. FIFO barrier
    // includes DiskOut's destructor's final partial-buffer write.
    if (!drain(timeoutMs))
        return failure(error, size, "Disk I/O did not drain in time; the buffer/file remains owned and open.");
    NRTLock(world);
    std::lock_guard<std::mutex> guard(ownersMutex);
    const auto found = owners.find({world, id});
    if (found == owners.end()) {
        NRTUnlock(world);
        return failure(error, size, "No managed disk buffer exists.");
    }
    auto& owner = *found->second;
    const int result = owner.file ? sf_close(owner.file) : 0;
    const auto ioError = owner.info.ioError;
    owner.file = nullptr;
    for (auto* table : {world->mSndBufs, world->mSndBufsNonRealTimeMirror})
        if (table[id].data == owner.ring.data()) table[id] = {};
    ++world->mSndBufUpdates[id].writes;
    NRTUnlock(world);
    owners.erase(found);
    return result == 0 && !ioError ? 1 : failure(error, size, "libsndfile reported a disk read/write/finalization error.");
}
static int inspect(World* world, unsigned id, DexterDiskBufferInfo* result) {
    std::lock_guard<std::mutex> guard(ownersMutex);
    const auto found = owners.find({world, id});
    if (!result || found == owners.end()) return 0;
    *result = found->second->info; return 1;
}
static std::uint64_t drops() { return gDiskIO ? gDiskIO->mDrops.load() : 0; }

struct HostUnit {
    World* world = nullptr;
    unsigned buffer = 0, channels = 0, fill = 0;
    int mode = 0;
    Unit* unit = nullptr;
    Graph graph{};
    Rate rate{};
    std::array<std::array<float, 64>, 4> input{}, output{};
    std::array<float*, 4> in{}, out{};
    std::array<Wire, 4> wires{};
    std::array<Wire*, 4> wirePointers{};
    std::uint64_t captured = 0, maximum = 0;
    bool stopped = true;
    void calculate() { unit->mCalcFunc(unit, 64); }
    void finish(bool manual) {
        if (stopped) return;
        if (mode == 3) {
            if (manual) {
                std::lock_guard<std::mutex> guard(ownersMutex);
                const auto found = owners.find({world, buffer});
                if (found != owners.end())
                    found->second->info.limitFrames =
                        std::min(found->second->info.limitFrames, captured);
            }
            if (fill) {
                for (unsigned ch = 1; ch <= channels; ++ch)
                    std::fill(input[ch].begin() + fill, input[ch].end(), 0.0f);
                calculate(); fill = 0;
            }
            DiskOut_Dtor(static_cast<DiskOut*>(unit));
        }
        stopped = true;
    }
    ~HostUnit() { if (unit) { finish(true); std::free(unit); } }
};
static void* createUnit(World* world, unsigned buffer, int mode, float playbackRate,
                        char* error, std::size_t errorSize) {
    try {
        DexterDiskBufferInfo info{};
        if (!inspect(world, buffer, &info) || world->mBufLength != 64 ||
            mode < 1 || mode > 3 || !std::isfinite(playbackRate) ||
            (mode == 2 && (playbackRate <= 0 || playbackRate > 8)))
            return failure(error, errorSize, "Disk unit needs a managed buffer, 64-frame engine blocks, and a forward rate in (0, 8]."), nullptr;
        {
            std::lock_guard<std::mutex> guard(ownersMutex);
            if (owners.at({world, buffer})->recording != (mode == 3))
                return failure(error, errorSize, "Disk unit does not match the buffer read/write mode."), nullptr;
        }
        auto host = std::make_unique<HostUnit>();
        host->world = world; host->buffer = buffer; host->channels = info.channels;
        host->mode = mode; host->maximum = info.limitFrames;
        const auto bytes = mode == 1 ? sizeof(DiskIn) : mode == 2 ? sizeof(VDiskIn) : sizeof(DiskOut);
        host->unit = static_cast<Unit*>(std::calloc(1, bytes));
        if (!host->unit) return failure(error, errorSize, "Could not allocate the disk Unit adapter."), nullptr;
        auto* unit = host->unit;
        host->rate.mSampleRate = world->mSampleRate;
        host->rate.mSampleDur = 1.0 / world->mSampleRate;
        host->rate.mBufLength = 64;
        host->rate.mSlopeFactor = 1.0 / 64;
        host->graph.mFullRate = &host->rate;
        host->graph.localBufNum = -1;
        unit->mWorld = world; unit->mParent = &host->graph; unit->mRate = &host->rate;
        unit->mBufLength = 64;
        unit->mNumInputs = mode == 1 ? 2 : mode == 2 ? 4 : 1 + info.channels;
        unit->mNumOutputs = mode == 3 ? 1 : info.channels;
        for (unsigned i = 0; i < 4; ++i) {
            host->in[i] = host->input[i].data(); host->out[i] = host->output[i].data();
            host->wires[i].mCalcRate = calc_ScalarRate;
            host->wirePointers[i] = &host->wires[i];
        }
        unit->mInBuf = host->in.data(); unit->mOutBuf = host->out.data();
        unit->mInput = host->wirePointers.data();
        host->input[0][0] = static_cast<float>(buffer);
        bool looping = false;
        {
            std::lock_guard<std::mutex> guard(ownersMutex);
            looping = owners.at({world, buffer})->looping;
        }
        if (mode == 1) {
            host->input[1][0] = looping ? 1.0f : 0.0f;
            DiskIn_Ctor(static_cast<DiskIn*>(unit));
        } else if (mode == 2) {
            host->input[1][0] = playbackRate;
            host->input[2][0] = looping ? 1.0f : 0.0f;
            VDiskIn_Ctor(static_cast<VDiskIn*>(unit));
        } else DiskOut_Ctor(static_cast<DiskOut*>(unit));
        host->stopped = false;
        return host.release();
    } catch (const std::exception& ex) { failure(error, errorSize, ex.what()); return nullptr; }
}
static void renderReader(void* raw, float* left, float* right, unsigned frames) {
    auto& host = *static_cast<HostUnit*>(raw);
    if (frames != 64 || host.mode == 3 || host.stopped || host.unit->mDone) {
        std::fill_n(left, frames, 0.0f); std::fill_n(right, frames, 0.0f); return;
    }
    host.calculate();
    std::copy_n(host.output[0].data(), frames, left);
    std::copy_n(host.output[host.channels == 1 ? 0 : 1].data(), frames, right);
}
static void captureSample(void* raw, float left, float right) {
    auto& host = *static_cast<HostUnit*>(raw);
    if (host.mode != 3 || host.stopped) return;
    host.input[1][host.fill] = left;
    if (host.channels == 2) host.input[2][host.fill] = right;
    ++host.fill; ++host.captured;
    if (host.fill == 64) { host.calculate(); host.fill = 0; }
    if (host.captured >= host.maximum) host.finish(false);
}
static void destroyUnit(void* raw) { delete static_cast<HostUnit*>(raw); }
} // namespace dexter_disk

extern "C" SC_API_EXPORT const DexterDiskIOApi* DexterDiskIO_GetApi() {
    static const DexterDiskIOApi api{1, sc_api_version, dexter_disk::openRead,
        dexter_disk::openWrite, dexter_disk::closeBuffer, dexter_disk::inspect, dexter_disk::drops,
        dexter_disk::createUnit, dexter_disk::renderReader, dexter_disk::captureSample, dexter_disk::destroyUnit};
    return &api;
}
