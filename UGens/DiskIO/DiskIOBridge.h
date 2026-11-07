// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

struct World;

// Private adapter ABI, not a replacement for the SuperCollider UGen API.
// Call from a non-audio worker while the caller owns its World lifecycle lock.
// Stop all graphs using a buffer before closeBuffer; close/drain it before
// World_Cleanup. UTF-8 paths are relative to the supplied portable root.
struct DexterDiskBufferInfo {
    std::uint32_t channels;
    std::uint32_t sampleRate;
    std::uint64_t fileFrames;
    std::uint64_t writtenFrames;
    std::uint64_t limitFrames;
    std::uint32_t ioError;
};

struct DexterDiskIOApi {
    std::uint32_t version;
    std::uint32_t scApiVersion;
    int (*openRead)(World*, std::uint32_t, const char* samplesRoot,
                    const char* relativeFile, int loop, DexterDiskBufferInfo*,
                    char* error, std::size_t errorSize);
    int (*openWrite)(World*, std::uint32_t, const char* recordingsRoot,
                     std::uint32_t channels, std::uint32_t sampleRate,
                     double maxSeconds, char* createdPath, std::size_t pathSize,
                     char* error, std::size_t errorSize);
    int (*closeBuffer)(World*, std::uint32_t, std::uint32_t timeoutMs,
                       char* error, std::size_t errorSize);
    int (*inspectBuffer)(World*, std::uint32_t, DexterDiskBufferInfo*);
    std::uint64_t (*droppedRequests)();
    // Exact upstream Unit DSP in a small host adapter; no GraphDef/sclang is
    // needed to operate the three fixed disk operations.
    void* (*createUnit)(World*, std::uint32_t, int mode, float rate,
                        char* error, std::size_t errorSize); // 1=DiskIn,2=VDiskIn,3=DiskOut
    void (*renderReader)(void*, float* left, float* right, std::uint32_t frames);
    void (*captureSample)(void*, float left, float right);
    void (*destroyUnit)(void*); // worker only, then closeBuffer to drain/finalize
};

using DexterDiskIOGetApi = const DexterDiskIOApi* (*)();
