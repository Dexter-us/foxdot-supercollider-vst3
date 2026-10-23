#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/ustring.h"

#include "SC_WorldOptions.h"
#include "SC_Prototypes.h"
#include "SC_HiddenWorld.h"
#include "SC_GraphDef.h"
#include "SC_Group.h"
#include "SC_Graph.h"
#include "SC_Lib_Cintf.h"
#include "tone_synthdef.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
std::mutex scLibraryLifecycleMutex;
Steinberg::ModuleTerminator cleanupSuperCollider([] { deinitialize_library(); });

std::filesystem::path pluginBinaryPath() {
#if defined(_WIN32)
    HMODULE module = nullptr;
    const auto address = reinterpret_cast<LPCWSTR>(&pluginBinaryPath);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            address, &module))
        return {};

    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        return {};
    return std::filesystem::path(std::wstring(path.data(), length));
#else
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&pluginBinaryPath), &info) == 0 || !info.dli_fname)
        return {};
    return std::filesystem::path(info.dli_fname);
#endif
}

std::filesystem::path bundledUGenPath() {
    const auto binaryPath = pluginBinaryPath();
    if (binaryPath.empty())
        return {};
    return binaryPath.parent_path().parent_path() / "Resources" / "SCPlugins";
}
}

namespace Steinberg::Vst {

class DexterSCTone final : public SingleComponentEffect {
public:
    static FUnknown* createInstance(void*) {
        return static_cast<IAudioProcessor*>(new DexterSCTone());
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
        const auto result = SingleComponentEffect::initialize(context);
        if (result != kResultOk)
            return result;
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        return kResultOk;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        const auto baseResult = SingleComponentEffect::setupProcessing(setup);
        if (baseResult != kResultOk)
            return baseResult;
        if (world_ && sampleRate_ == setup.sampleRate)
            return kResultOk;
        stopWorld();
        sampleRate_ = setup.sampleRate;
        return initializeWorld() ? kResultOk : kResultFalse;
    }

    tresult PLUGIN_API terminate() override {
        stopWorld();
        return SingleComponentEffect::terminate();
    }

    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API process(ProcessData& data) override {
        if (data.numOutputs < 1 || !data.outputs)
            return kResultOk;
        auto& output = data.outputs[0];
        if (output.numChannels <= 0 || !output.channelBuffers32)
            return kResultOk;
        if (!world_)
            return kResultFalse;

        for (int32 channel = 0; channel < output.numChannels; ++channel) {
            if (output.channelBuffers32[channel])
                std::fill_n(output.channelBuffers32[channel], data.numSamples, 0.0f);
        }

        for (int32 frame = 0; frame < data.numSamples; ++frame) {
            if (cursor_ == kSCBlockSize)
                renderSCBlock();
            const float sample = outputBlock_[cursor_++];
            for (int32 channel = 0; channel < output.numChannels; ++channel) {
                if (output.channelBuffers32[channel])
                    output.channelBuffers32[channel][frame] = sample;
            }
        }
        output.silenceFlags = 0;
        return kResultOk;
    }

    OBJ_METHODS(DexterSCTone, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    static constexpr int kSCBlockSize = 64;

    bool initializeWorld() {
        std::lock_guard<std::mutex> lifecycleLock(scLibraryLifecycleMutex);
        const auto ugenPath = bundledUGenPath();
        std::error_code pathError;
        if (ugenPath.empty() || !std::filesystem::is_directory(ugenPath, pathError))
            return false;
        ugenPluginPath_ = ugenPath.u8string();

        WorldOptions options;
        options.mRealTime = false;
        options.mPreferredSampleRate = sampleRate_;
        options.mBufLength = kSCBlockSize;
        options.mNumInputBusChannels = 0;
        options.mNumOutputBusChannels = 2;
        options.mNumAudioBusChannels = 32;
        options.mLoadGraphDefs = 0;
        options.mRendezvous = false;
        options.mVerbosity = -1;
        options.mUGensPluginPath = ugenPluginPath_.c_str();

        world_ = World_New(&options);
        if (!world_)
            return false;
        World_SetSampleRate(world_, sampleRate_);
        World_Start(world_);

        auto* defs = GraphDef_Recv(world_, reinterpret_cast<const char*>(kToneSynthDef),
                                   sizeof(kToneSynthDef), nullptr);
        if (!defs) {
            discardPartialWorld();
            return false;
        }
        GraphDef_Define(world_, defs);

        int32 synthName[4]{};
        std::memcpy(synthName, "DexterTone", 10);
        auto* definition = World_GetGraphDef(world_, synthName);
        auto* root = World_GetGroup(world_, 0);
        if (!definition || !root) {
            discardPartialWorld();
            return false;
        }
        sc_msg_iter noControls(0, "");
        Graph* synth = nullptr;
        if (Graph_New(world_, definition, 100, &noControls, &synth, true) != 0 || !synth) {
            discardPartialWorld();
            return false;
        }
        Group_AddTail(root, &synth->mNode);
        cursor_ = kSCBlockSize;
        return true;
    }

    void discardPartialWorld() {
        if (world_) {
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
    }

    void renderSCBlock() {
        World_Run(world_);
        std::copy_n(world_->mAudioBus, kSCBlockSize, outputBlock_.begin());
        ++world_->mBufCounter;
        cursor_ = 0;
    }

    // UGen registration is process-global in this SC revision. Keep modules loaded
    // while any VST instance exists; the VST module terminator unloads them once.
    void stopWorld() {
        if (world_) {
            std::lock_guard<std::mutex> lifecycleLock(scLibraryLifecycleMutex);
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        cursor_ = kSCBlockSize;
        outputBlock_.fill(0.0f);
    }

    double sampleRate_ = 48000.0;
    World* world_ = nullptr;
    std::string ugenPluginPath_;
    std::array<float, kSCBlockSize> outputBlock_{};
    int cursor_ = kSCBlockSize;
};

} // namespace Steinberg::Vst

BEGIN_FACTORY_DEF("Dexter U.S.", "", "")

DEF_CLASS2(INLINE_UID(0xC7F03BC1, 0xDEEC47AA, 0x8E28B107, 0xF9B1A553),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           "Dexter SC Tone Spike",
           0,
           "Instrument|Synth",
           "0.1.0",
           kVstVersionString,
           Steinberg::Vst::DexterSCTone::createInstance)

END_FACTORY
