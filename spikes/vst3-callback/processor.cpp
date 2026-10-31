#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "SC_WorldOptions.h"
#include "SC_Prototypes.h"
#include "SC_World.h"
#include "SC_GraphDef.h"
#include "SC_Group.h"
#include "SC_Graph.h"
#include "SC_Lib_Cintf.h"
#include "tone_synthdef.h"
#include "foxdot_synthdefs.h"
#include "sample_synthdef.h"
#include "portable_samples.h"
#include "reverb_synthdef.h"
#include "python_runtime.h"
#include "win32_code_editor.h"
#include "lifecycle_trace.h"
#include "sc_pool_capacity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
std::mutex scLibraryLifecycleMutex;
std::atomic<unsigned long long> nextScriptNamespace{1};
Steinberg::ModuleTerminator cleanupSuperCollider([] {
    LifecycleTrace trace("deinitialize_library");
    deinitialize_library();
});

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

std::filesystem::path portableUGenPath() {
    const auto binaryPath = pluginBinaryPath();
    if (binaryPath.empty())
        return {};
    return binaryPath.parent_path().parent_path().parent_path() / "Portable" / "SCPlugins";
}

std::vector<std::string> discoverUGenModuleFiles() {
    std::vector<std::string> modules;
    for (const auto& directory : {bundledUGenPath(), portableUGenPath()}) {
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
            continue;
        std::filesystem::directory_iterator iterator(directory, error);
        const std::filesystem::directory_iterator end;
        for (; !error && iterator != end; iterator.increment(error)) {
            std::error_code fileError;
            if (!iterator->is_regular_file(fileError) || fileError)
                continue;
            auto extension = iterator->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".scx")
                modules.push_back(iterator->path().stem().u8string());
        }
    }
    std::sort(modules.begin(), modules.end());
    modules.erase(std::unique(modules.begin(), modules.end()), modules.end());
    return modules;
}

std::string ugenModuleSearchPath() {
    std::string path = bundledUGenPath().u8string();
    const auto portablePath = portableUGenPath();
    std::error_code error;
    if (!portablePath.empty() && std::filesystem::is_directory(portablePath, error)) {
#if defined(_WIN32)
        path += ';';
#else
        path += ':';
#endif
        path += portablePath.u8string();
    }
    return path;
}

std::filesystem::path bundledPythonPath() {
    const auto binaryPath = pluginBinaryPath();
    if (binaryPath.empty())
        return {};
    return binaryPath.parent_path().parent_path() / "Resources" / "Python";
}
}

namespace Steinberg::Vst {

class DexterSCTone final : public SingleComponentEffect, public ICodeEditorHost {
public:
    static FUnknown* createInstance(void*) {
        LifecycleTrace trace("createInstance");
        return static_cast<IAudioProcessor*>(new DexterSCTone());
    }

    DexterSCTone()
        : scriptNamespace_("_dexter_instance_" +
                           std::to_string(nextScriptNamespace.fetch_add(1))) {}

    ~DexterSCTone() override {
        LifecycleTrace trace("processor destructor", this);
        joinScriptThread();
        stopWorld();
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
        LifecycleTrace trace("processor initialize", this);
        const auto result = SingleComponentEffect::initialize(context);
        if (result != kResultOk)
            return result;
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        addEventOutput(STR16("MIDI Out"), 16);
        return kResultOk;
    }

    IPlugView* PLUGIN_API createView(FIDString name) override {
        LifecycleTrace trace("createView", this);
        if (!name || std::strcmp(name, ViewType::kEditor) != 0)
            return nullptr;
        return createCodeEditorView(
            this, static_cast<IEditController*>(this));
    }

    tresult PLUGIN_API getState(IBStream* stream) override {
        if (!stream)
            return kInvalidArgument;

        std::string source;
        {
            std::lock_guard<std::mutex> lock(editorStateMutex_);
            source = editorSource_;
        }
        if (source.size() > kMaxEditorSourceBytes)
            return kResultFalse;

        std::array<char, 4> magic{'D', 'X', 'C', '1'};
        std::array<char, 4> length{};
        encodeUint32(static_cast<std::uint32_t>(source.size()), length);

        if (!writeStreamBytes(stream, magic.data(), static_cast<int32>(magic.size())) ||
            !writeStreamBytes(stream, length.data(), static_cast<int32>(length.size())) ||
            (!source.empty() &&
             !writeStreamBytes(stream, source.data(), static_cast<int32>(source.size()))))
            return kResultFalse;
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* stream) override {
        if (!stream)
            return kInvalidArgument;

        std::array<char, 4> magic{};
        int32 bytesRead = 0;
        while (bytesRead < static_cast<int32>(magic.size())) {
            int32 chunkRead = 0;
            if (stream->read(magic.data() + bytesRead,
                             static_cast<int32>(magic.size()) - bytesRead,
                             &chunkRead) != kResultOk)
                return kResultFalse;
            if (chunkRead == 0)
                return bytesRead == 0 ? kResultOk : kResultFalse;
            bytesRead += chunkRead;
        }
        if (magic != std::array<char, 4>{'D', 'X', 'C', '1'})
            return kResultFalse;

        std::array<char, 4> length{};
        if (!readStreamBytes(stream, length.data(), static_cast<int32>(length.size())))
            return kResultFalse;
        const auto sourceLength = decodeUint32(length);
        if (sourceLength > kMaxEditorSourceBytes)
            return kResultFalse;

        std::string source(sourceLength, '\0');
        if (!source.empty() &&
            !readStreamBytes(stream, source.data(), static_cast<int32>(source.size())))
            return kResultFalse;
        if (source.find('\0') != std::string::npos)
            return kResultFalse;

        {
            std::lock_guard<std::mutex> lock(editorStateMutex_);
            editorSource_ = std::move(source);
            hasEditorSource_ = true;
        }
        stopPattern();
        return kResultOk;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        LifecycleTrace trace("setupProcessing", this);
        const auto baseResult = SingleComponentEffect::setupProcessing(setup);
        if (baseResult != kResultOk)
            return baseResult;
        if (!std::isfinite(setup.sampleRate) || setup.sampleRate <= 0.0)
            return kInvalidArgument;
        return initializeWorld(setup.sampleRate) ? kResultOk : kResultFalse;
    }

    tresult PLUGIN_API terminate() override {
        LifecycleTrace trace("processor terminate", this);
        joinScriptThread();
        stopWorld();
        return SingleComponentEffect::terminate();
    }

    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API process(ProcessData& data) {
        if (data.numOutputs < 1 || !data.outputs)
            return kResultOk;
        auto& output = data.outputs[0];
        if (output.numChannels <= 0 || !output.channelBuffers32)
            return kResultOk;
        for (int32 channel = 0; channel < output.numChannels; ++channel) {
            if (output.channelBuffers32[channel])
                std::fill_n(output.channelBuffers32[channel], data.numSamples, 0.0f);
        }
        // Script preparation/initialization owns the world off the audio thread.
        // New-bank note nodes use SC's bounded RT pool while rendering.
        // If preparation briefly owns the world, output silence for this
        // block rather than blocking the host's audio callback.
        std::unique_lock<std::mutex> worldLock(worldAccessMutex_, std::try_to_lock);
        if (!worldLock.owns_lock()) {
            output.silenceFlags = 1;
            return kResultOk;
        }
        if (!world_)
            return kResultFalse;

        std::unique_lock<std::mutex> patternLock(patternMutex_, std::try_to_lock);
        if (patternLock.owns_lock()) {
            const auto updateMask = pendingPlayerMask_;
            const auto muteMask = pendingMuteMask_;
            if (updateMask != 0) {
                const auto newStart = sampleClock_ +
                    static_cast<std::uint64_t>(kSCBlockSize - cursor_);
                for (std::size_t player = 0; player < kPlayerCount; ++player) {
                    if ((updateMask & static_cast<std::uint8_t>(1u << player)) == 0)
                        continue;
                    stopPlayerMidiNotes(data.outputEvents, 0, static_cast<int>(player));
                    stopAudioVoices(static_cast<int>(player));
                    const auto previous = activePatterns_[player];
                    activePatterns_[player] = pendingPatterns_[player];
                    if (!activePatterns_[player].active) {
                        activePatterns_[player].echoMix = previous.echoMix;
                        activePatterns_[player].echoBeats = previous.echoBeats;
                        activePatterns_[player].delayTimeSeconds = previous.delayTimeSeconds;
                        activePatterns_[player].echoFeedback = previous.echoFeedback;
                        activePatterns_[player].echoControl = previous.echoControl;
                        activePatterns_[player].delayControl = previous.delayControl;
                        activePatterns_[player].echoMixControl = previous.echoMixControl;
                        activePatterns_[player].echoFeedbackControl = previous.echoFeedbackControl;
                        activePatterns_[player].noteDelayControl = previous.noteDelayControl;
                        activePatterns_[player].echoDecayControl = previous.echoDecayControl;
                        activePatterns_[player].echoDecayBeats = previous.echoDecayBeats;
                        activePatterns_[player].reverbIndex = previous.reverbIndex;
                        activePatterns_[player].reverbMix = previous.reverbMix;
                        activePatterns_[player].reverbRoom = previous.reverbRoom;
                        activePatterns_[player].reverbDamp = previous.reverbDamp;
                        activePatterns_[player].reverbTime = previous.reverbTime;
                        activePatterns_[player].reverbSpread = previous.reverbSpread;
                        activePatterns_[player].lpfHz = previous.lpfHz;
                        activePatterns_[player].lpr = previous.lpr;
                        activePatterns_[player].hpfHz = previous.hpfHz;
                        activePatterns_[player].hpr = previous.hpr;
                        activePatterns_[player].bpfHz = previous.bpfHz;
                        activePatterns_[player].bpr = previous.bpr;
                        activePatterns_[player].lpfControl = previous.lpfControl;
                        activePatterns_[player].lprControl = previous.lprControl;
                        activePatterns_[player].hpfControl = previous.hpfControl;
                        activePatterns_[player].hprControl = previous.hprControl;
                        activePatterns_[player].bpfControl = previous.bpfControl;
                        activePatterns_[player].bprControl = previous.bprControl;
                        activePatterns_[player].bpnoise = previous.bpnoise;
                        activePatterns_[player].swell = previous.swell;
                        activePatterns_[player].bpnoiseControl = previous.bpnoiseControl;
                        activePatterns_[player].swellControl = previous.swellControl;
                    }
                    patternStartSamples_[player] = newStart;
                    updatePlayerReverb(player);
                }
            }
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                if ((muteMask & static_cast<std::uint8_t>(1u << player)) == 0)
                    continue;
                const bool wasMuted = mutedPlayers_[player];
                mutedPlayers_[player] = pendingMuteStates_[player];
                if (!wasMuted && mutedPlayers_[player])
                    stopPlayerMidiNotes(data.outputEvents, 0, static_cast<int>(player));
            }
            pendingPlayerMask_ = 0;
            pendingMuteMask_ = 0;
            patternLock.unlock();
        }

        if (stopRequested_.exchange(false, std::memory_order_acq_rel)) {
            stopActiveMidiNotes(data.outputEvents, 0);
            stopAudioVoices();
            for (auto& pattern : activePatterns_)
                pattern.active = false;
        }

        emitPatternMidi(data);
        for (int32 frame = 0; frame < data.numSamples; ++frame) {
            if (cursor_ == kSCBlockSize) {
                const auto blockStart = sampleClock_ + static_cast<std::uint64_t>(frame);
                updateSynthForBlock(blockStart);
                renderSCBlock(blockStart);
            }
            const auto outputSample = static_cast<std::size_t>(cursor_++);
            const float leftSample = outputBlock_[0][outputSample];
            const float rightSample = outputBlock_[1][outputSample];
            for (int32 channel = 0; channel < output.numChannels; ++channel) {
                if (!output.channelBuffers32[channel])
                    continue;
                const float sample = channel == 0 ? leftSample
                                    : channel == 1 ? rightSample
                                                   : 0.5f * (leftSample + rightSample);
                output.channelBuffers32[channel][frame] = sample;
            }
        }
        sampleClock_ += static_cast<std::uint64_t>(data.numSamples);
        output.silenceFlags = 0;
        return kResultOk;
    }

    std::string checkCompatibility(const std::string& source) override {
        if (source.size() > kMaxEditorSourceBytes)
            return "Code is limited to 64 KB in this prototype.";
        std::lock_guard<std::mutex> threadLock(scriptThreadMutex_);
        if (scriptRunning_.load(std::memory_order_acquire))
            return "Python is evaluating another script; wait for it to finish, then check again.";
        if (scriptThread_.joinable())
            scriptThread_.join();

        std::string report;
        std::string error;
        if (!PortablePython::instance().checkCompatibility(bundledPythonPath(), source,
                                                          report, error)) {
            return error.empty() ? "FoxDot compatibility check failed." : error;
        }
        setEditorStatus("Static compatibility check complete; the script was not run.");
        return report;
    }

    void submitCode(const std::string& source) override {
        std::lock_guard<std::mutex> threadLock(scriptThreadMutex_);
        if (scriptRunning_.load(std::memory_order_acquire)) {
            setEditorStatus("Python is still evaluating the previous script.");
            return;
        }
        if (scriptThread_.joinable())
            scriptThread_.join();

        const auto generation = scriptGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;
        scriptRunning_.store(true, std::memory_order_release);
        setEditorStatus("Evaluating Python...");
        try {
            scriptThread_ = std::thread([this, source, generation] {
                try {
                    PythonPattern pattern;
                    std::string error;
                    if (!PortablePython::instance().evaluate(bundledPythonPath(),
                                                             scriptNamespace_, source,
                                                             pattern, error)) {
                        auto message = error;
                        while (!message.empty() &&
                               (message.back() == '\r' || message.back() == '\n'))
                            message.pop_back();
                        setEditorStatus(message.empty() ? "Python evaluation failed."
                                                        : message);
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }
                    if (scriptGeneration_.load(std::memory_order_acquire) != generation) {
                        setEditorStatus("Evaluation cancelled.");
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }
                    if (pattern.players.empty() && pattern.mutes.empty()) {
                        setEditorStatus("No players changed.");
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }

                    std::array<PatternState, kPlayerCount> updates{};
                    std::uint8_t updateMask = 0;
                    std::size_t totalNotes = 0;
                    for (const auto& player : pattern.players) {
                        if (player.playerIndex < 0 ||
                            player.playerIndex >= static_cast<int>(kPlayerCount) ||
                            player.events.size() > kMaxPatternEvents) {
                            setEditorStatus("Player update exceeds the supported limit.");
                            scriptRunning_.store(false, std::memory_order_release);
                            return;
                        }
                        auto& next = updates[static_cast<std::size_t>(player.playerIndex)];
                        next.active = !player.stopped;
                        next.bpm = player.bpm;
                        next.lengthBeats = player.lengthBeats;
                        next.synthIndex = player.synthIndex;
                        next.synthRate = static_cast<float>(player.synthRate);
                        next.synthControls = player.synthControls;
                        next.amplitude = static_cast<float>(player.amplitude);
                        next.pan = static_cast<float>(player.pan);
                        next.cutoffHz = static_cast<float>(player.cutoffHz);
                        next.mousePan = player.mousePan;
                        next.mouseCutoff = player.mouseCutoff;
                        next.attackSeconds = player.attackSeconds;
                        next.releaseSeconds = player.releaseSeconds;
                        if (!parseTimeVarControl(player.ampControlToken, 0.0, 1.0, next.ampControl) ||
                            !parseTimeVarControl(player.panControlToken, -1.0, 1.0, next.panControl) ||
                            !parseTimeVarControl(player.cutoffControlToken, 20.0, 20000.0, next.cutoffControl) ||
                            !parseTimeVarControl(player.echoControlToken, 0.0, 64.0, next.echoControl) ||
                            !parseTimeVarControl(player.delayTimeControlToken, 0.0, 4.0, next.delayControl) ||
                             !parseTimeVarControl(player.echoMixControlToken, 0.0, 1.0, next.echoMixControl) ||
                             !parseTimeVarControl(player.echoFeedbackControlToken, 0.0, 0.95, next.echoFeedbackControl) ||
                             !parseTimeVarControl(player.noteDelayControlToken, 0.0, 8.0, next.noteDelayControl) ||
                             !parseTimeVarControl(player.echoDecayControlToken, 0.0, 64.0, next.echoDecayControl) ||
                            !parseTimeVarControl(player.lpfControlToken, 0.0, 20000.0, next.lpfControl) ||
                            !parseTimeVarControl(player.lprControlToken, 0.05, 1.0, next.lprControl) ||
                            !parseTimeVarControl(player.hpfControlToken, 0.0, 20000.0, next.hpfControl) ||
                            !parseTimeVarControl(player.hprControlToken, 0.05, 1.0, next.hprControl) ||
                            !parseTimeVarControl(player.bpfControlToken, 0.0, 20000.0, next.bpfControl) ||
                            !parseTimeVarControl(player.bprControlToken, 0.05, 1.0, next.bprControl) ||
                            !parseTimeVarControl(player.bpnoiseControlToken, 0.0, 10.0, next.bpnoiseControl) ||
                            !parseTimeVarControl(player.swellControlToken, 0.0, 4.0, next.swellControl))
                            throw std::runtime_error("Invalid var/linvar control from Python.");
                        if (next.synthIndex == 57) {
                            if (!parseTimeVarControl(player.sampleControlToken, -1000000.0, 1000000.0,
                                                     next.sampleControl) ||
                                !parseTimeVarControl(player.sampleRateControlToken, -8.0, 8.0,
                                                     next.sampleRateControl))
                                throw std::runtime_error("Invalid sample/rate var/linvar control from Python.");
                            if (!validSampleSelectionControl(next.sampleControl))
                                throw std::runtime_error("Sample var/linvar requires whole sample indexes.");
                            if (!validSampleRateControl(next.sampleRateControl))
                                throw std::runtime_error("Rate var/linvar magnitude must be 0.0625 to 8; rate linvar must not cross zero.");
                        }
                        next.echoMix = static_cast<float>(player.echoMix);
                        next.echoBeats = player.echoBeats;
                         next.delayTimeSeconds = player.delayTimeSeconds;
                         next.echoFeedback = static_cast<float>(player.echoFeedback);
                        next.echoDecayBeats = player.echoDecayBeats;
                        next.reverbIndex = player.reverbIndex;
                        next.reverbMix = static_cast<float>(player.reverbMix);
                        next.reverbRoom = static_cast<float>(player.reverbRoom);
                        next.reverbDamp = static_cast<float>(player.reverbDamp);
                        next.reverbTime = static_cast<float>(player.reverbTime);
                        next.reverbSpread = static_cast<float>(player.reverbSpread);
                        next.lpfHz = static_cast<float>(player.lpfHz);
                        next.lpr = static_cast<float>(player.lpr);
                        next.hpfHz = static_cast<float>(player.hpfHz);
                        next.hpr = static_cast<float>(player.hpr);
                        next.bpfHz = static_cast<float>(player.bpfHz);
                        next.bpr = static_cast<float>(player.bpr);
                        next.bpnoise = static_cast<float>(player.bpnoise);
                        next.swell = static_cast<float>(player.swell);
                        next.eventCount = player.events.size();
                        std::copy(player.events.begin(), player.events.end(), next.events.begin());
                        totalNotes += player.events.size();
                        updateMask |= static_cast<std::uint8_t>(1u << player.playerIndex);
                    }
                    if (updateMask != 0) {
                        std::string graphError;
                        if (!prepareSampleBuffers(updates, updateMask, graphError)) {
                            setEditorStatus(graphError);
                            scriptRunning_.store(false, std::memory_order_release);
                            return;
                        }
                        if (!prepareSynthGraphs(updates, updateMask, graphError)) {
                            setEditorStatus(graphError.empty()
                                                ? "Could not prepare the selected synth graphs."
                                                : graphError);
                            scriptRunning_.store(false, std::memory_order_release);
                            return;
                        }
                    }
                    {
                        std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
                        if (scriptGeneration_.load(std::memory_order_acquire) != generation) {
                            setEditorStatus("Evaluation cancelled.");
                            scriptRunning_.store(false, std::memory_order_release);
                            return;
                        }
                        for (std::size_t player = 0; player < kPlayerCount; ++player) {
                            if ((updateMask & (1u << player)) && updates[player].sampleWorld != 0 &&
                                updates[player].sampleWorld != sampleWorldGeneration_)
                                throw std::runtime_error("The audio engine restarted while loading samples. Press Run to retry.");
                        }
                        std::lock_guard<std::mutex> lock(patternMutex_);
                        for (std::size_t player = 0; player < kPlayerCount; ++player) {
                            if (updateMask & static_cast<std::uint8_t>(1u << player))
                                pendingPatterns_[player] = updates[player];
                        }
                        pendingPlayerMask_ |= updateMask;
                        for (const auto& mute : pattern.mutes) {
                            if (mute.playerIndex < 0 || mute.playerIndex >= static_cast<int>(kPlayerCount)) {
                                setEditorStatus("Player mute control is outside p1 through p8.");
                                scriptRunning_.store(false, std::memory_order_release);
                                return;
                            }
                            const auto player = static_cast<std::size_t>(mute.playerIndex);
                            pendingMuteStates_[player] = mute.muted;
                            pendingMuteMask_ |= static_cast<std::uint8_t>(1u << player);
                        }
                    }
                    std::ostringstream status;
                    if (!pattern.players.empty()) {
                        status << "Updated " << pattern.players.size() << " player(s), "
                               << totalNotes << " notes at " << pattern.players.front().bpm << " BPM.";
                    } else {
                        status << "Player control updated.";
                    }
                    for (const auto& mute : pattern.mutes)
                        status << " p" << mute.playerIndex + 1 << (mute.muted ? " muted." : " unmuted.");
                    setEditorStatus(status.str());
                } catch (const std::exception& exception) {
                    setEditorStatus(std::string("Python worker error: ") + exception.what());
                } catch (...) {
                    setEditorStatus("Python worker failed unexpectedly.");
                }
                scriptRunning_.store(false, std::memory_order_release);
            });
        } catch (const std::exception& exception) {
            scriptRunning_.store(false, std::memory_order_release);
            setEditorStatus(std::string("Could not start Python worker: ") + exception.what());
        }
    }

    void stopPattern() override {
        scriptGeneration_.fetch_add(1, std::memory_order_acq_rel);
        stopRequested_.store(true, std::memory_order_release);
        setEditorStatus("Stopping pattern...");
    }

    std::string editorSource() override {
        std::lock_guard<std::mutex> lock(editorStateMutex_);
        return editorSource_;
    }

    bool hasEditorSource() override {
        std::lock_guard<std::mutex> lock(editorStateMutex_);
        return hasEditorSource_;
    }

    void setEditorSource(const std::string& source) override {
        if (source.size() > kMaxEditorSourceBytes)
            return;
        std::lock_guard<std::mutex> lock(editorStateMutex_);
        editorSource_ = source;
        hasEditorSource_ = true;
    }

    std::string editorStatus() override {
        std::lock_guard<std::mutex> lock(statusMutex_);
        return editorStatus_;
    }

    OBJ_METHODS(DexterSCTone, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    static constexpr int kSCBlockSize = 64;
    static constexpr std::size_t kMaxPatternEvents = 256;
    static constexpr std::size_t kPlayerCount = 8;
    static constexpr std::size_t kMaxPolyphony = 8;
    static constexpr std::size_t kVoiceSlotsPerPlayer = kMaxPolyphony * 2 + 1;
    static constexpr std::size_t kAudioVoiceCount =
        kPlayerCount * kVoiceSlotsPerPlayer;
    static constexpr std::size_t kMaxCachedSynthGraphsPerVoice = 2;
    static constexpr auto kSynthGraphNames = std::array{
        "DexterTone", "DexterBass", "DexterVarsaw", "DexterDirt",
        "DexterJBass", "DexterNoise", "DexterDub", "DexterGrowl",
        "DexterSoprano", "DexterCrunch", "DexterLazer", "DexterRave",
        "DexterBell", "DexterGong", "DexterMarimba", "DexterPluck",
        "DexterTwang", "DexterDonk", "DexterSquish", "DexterCharm",
        "DexterSpark", "DexterBlip", "DexterScatter", "DexterOrient",
        "DexterZap", "DexterFuzz", "DexterBug", "DexterRipple",
        "DexterViola", "DexterNylon", "DexterArpy", "DexterDab",
        "DexterSawbass", "DexterDbass", "DexterProphet", "DexterPads",
        "DexterPasha", "DexterAmbi", "DexterSinepad", "DexterKeys",
        "DexterKlank", "DexterFeel", "DexterGlass", "DexterSoft",
        "DexterQuin", "DexterSpace", "DexterScratch", "DexterCreep",
        "DexterSnick", "DexterSwell", "DexterRazz", "DexterSitar",
        "DexterStar", "DexterKarp", "DexterSample"};
    static constexpr std::size_t kSynthGraphCount = kSynthGraphNames.size();
    // The first graph handles sine/triangle/saw/square; every later synth
    // index owns one graph. This table grows automatically with the graph bank.
    static constexpr auto kSynthIndexToGraphIndex = [] {
        std::array<std::size_t, kSynthGraphCount + 3> mapping{};
        for (std::size_t index = 0; index < mapping.size(); ++index)
            mapping[index] = index <= 3 ? 0 : index - 3;
        return mapping;
    }();
    static_assert(kSynthIndexToGraphIndex.front() == 0 &&
                      kSynthIndexToGraphIndex.back() == kSynthGraphCount - 1,
                  "Every Python synth index must resolve to a registered graph.");
    static constexpr std::size_t kReverbGraphCount = 3;
    static constexpr std::size_t kCalculatedWorldNodeCapacity =
        kAudioVoiceCount * kMaxCachedSynthGraphsPerVoice +
        kPlayerCount * kReverbGraphCount + 1;
    // Preserve the known-good headroom; graph definitions are registered
    // independently, while active voice graphs are created lazily and cached.
    static constexpr std::size_t kWorldNodeCapacity =
        scPoolCapacity(kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity);
    static constexpr std::size_t kWorldGraphDefCapacity =
        scPoolCapacity(kSynthGraphCount + kReverbGraphCount);
    static_assert((kWorldNodeCapacity & (kWorldNodeCapacity - 1)) == 0,
                  "SC node hash capacity must be a power of two.");
    static_assert((kWorldGraphDefCapacity & (kWorldGraphDefCapacity - 1)) == 0,
                  "SC graph-definition hash capacity must be a power of two.");
    static constexpr int kFirstAudioBus = 2;
    static constexpr int kAudioBusStride = 3; // audio, MouseX, MouseY
    static constexpr int kReverbBusStride = 4; // stereo send, stereo return
    static constexpr int kReverbBaseBus = kFirstAudioBus + static_cast<int>(kAudioVoiceCount) * kAudioBusStride;
    static constexpr int kNumAudioBusChannels = 512;
    static constexpr double kAttackSeconds = 0.010;
    static constexpr double kReleaseSeconds = 0.040;
    static constexpr double kReverbSendReleaseSeconds = 0.015;
    static constexpr double kMixSmoothingSeconds = 0.010;
    static constexpr std::size_t kMaxEchoBufferSamples = 96000;
    static constexpr double kPi = 3.14159265358979323846;
    static_assert(kFirstAudioBus + kAudioVoiceCount * kAudioBusStride <= kNumAudioBusChannels,
                  "The SuperCollider world needs three private buses per synth voice.");
    static_assert(kReverbBaseBus + kPlayerCount * kReverbBusStride <= kNumAudioBusChannels,
                  "The SuperCollider world needs three private buses per player reverb.");
    static_assert(kWorldNodeCapacity <= 0xffffffffu,
                  "The SuperCollider world node capacity must fit WorldOptions.");
    static_assert(kWorldNodeCapacity >= 2048,
                  "Preserve the proven minimum node-pool headroom.");
    static_assert(kWorldGraphDefCapacity <= 0xffffffffu,
                  "The SuperCollider graph-definition capacity must fit WorldOptions.");
    static_assert(kWorldNodeCapacity + 100 <= 0x7fffffff,
                  "The SuperCollider node IDs must fit Graph_New's signed ID.");
    static constexpr std::size_t kMaxMidiEventsPerBlock = 1024;
    static constexpr std::size_t kMaxEditorSourceBytes = 256 * 1024;

    static void encodeUint32(std::uint32_t value, std::array<char, 4>& bytes) {
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<char>((value >> (i * 8)) & 0xffu);
    }

    static std::uint32_t decodeUint32(const std::array<char, 4>& bytes) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i)
            value |= static_cast<std::uint32_t>(
                         static_cast<unsigned char>(bytes[i])) << (i * 8);
        return value;
    }

    static bool writeStreamBytes(IBStream* stream, const void* bytes, int32 size) {
        auto* data = const_cast<char*>(static_cast<const char*>(bytes));
        int32 offset = 0;
        while (offset < size) {
            int32 written = 0;
            if (stream->write(data + offset, size - offset, &written) != kResultOk ||
                written <= 0)
                return false;
            offset += written;
        }
        return true;
    }

    static bool readStreamBytes(IBStream* stream, void* bytes, int32 size) {
        auto* data = static_cast<char*>(bytes);
        int32 offset = 0;
        while (offset < size) {
            int32 read = 0;
            if (stream->read(data + offset, size - offset, &read) != kResultOk ||
                read <= 0)
                return false;
            offset += read;
        }
        return true;
    }

    struct TimeVarControl {
        bool active = false;
        bool linear = false;
        std::size_t count = 0;
        std::array<double, 8> values{};
        std::array<double, 8> durations{};
    };

    struct PatternState {
        std::array<PythonNoteEvent, kMaxPatternEvents> events{};
        std::size_t eventCount = 0;
        std::uint64_t sampleWorld = 0;
        TimeVarControl sampleControl;
        TimeVarControl sampleRateControl;
        double bpm = 120.0;
        double lengthBeats = 1.0;
        int synthIndex = 0;
        float synthRate = 0.0f;
        std::array<double, 7> synthControls{0, 0.5, 1, 0.5, 0.5, 0, 1};
        float amplitude = 1.0f;
        float pan = 0.0f;
        float cutoffHz = 20000.0f;
        bool mousePan = false;
        bool mouseCutoff = false;
        double attackSeconds = kAttackSeconds;
        double releaseSeconds = kReleaseSeconds;
        TimeVarControl ampControl;
        TimeVarControl panControl;
        TimeVarControl cutoffControl;
        TimeVarControl lpfControl;
        TimeVarControl lprControl;
        TimeVarControl hpfControl;
        TimeVarControl hprControl;
        TimeVarControl bpfControl;
        TimeVarControl bprControl;
        TimeVarControl bpnoiseControl;
        TimeVarControl swellControl;
        TimeVarControl echoControl;
        TimeVarControl delayControl;
        TimeVarControl echoMixControl;
        TimeVarControl echoFeedbackControl;
        TimeVarControl noteDelayControl;
        TimeVarControl echoDecayControl;
        double echoBeats = 0.0;
        double delayTimeSeconds = 0.0;
        float echoMix = 0.35f;
        float echoFeedback = -1.0f;
        double echoDecayBeats = 1.0;
        int reverbIndex = 1;
        float reverbMix = 0.1f;
        float reverbRoom = 0.0f;
        float reverbDamp = 0.5f;
        float reverbTime = 3.0f;
        float reverbSpread = 15.0f;
        float lpfHz = 0.0f;
        float lpr = 1.0f;
        float hpfHz = 0.0f;
        float hpr = 1.0f;
        float bpfHz = 0.0f;
        float bpr = 1.0f;
        float bpnoise = 0.0f;
        float swell = 0.0f;
        bool active = false;
    };

    struct TimedMidiEvent {
        std::uint64_t sample = 0;
        int32 noteId = -1;
        int playerIndex = 0;
        int midiNote = 60;
        int velocity = 100;
        bool noteOn = false;
    };

    struct ActiveMidiGate {
        int32 noteId = -1;
        int midiNote = -1;
        bool emitted = false;
    };

    struct BiquadState {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        float cutoffHz = -1.0f;
        float rq = -1.0f;
        int mode = -1;
        bool enabled = false;
        float wet = 0.0f;
    };

    struct ModulationNoiseState {
        double phase = 0.0;
        float from = 0.0f;
        float to = 0.0f;
        std::uint32_t randomState = 1;
    };

    struct AudioVoice {
        std::array<Graph*, kSynthGraphCount> synthGraphs{};
        Graph* activeSynth = nullptr;
        int playerIndex = -1;
        int midiNote = -1;
        int velocity = 0;
        int synthIndex = 0;
        float amplitude = 1.0f;
        float pan = 0.0f;
        float leftGain = 0.70710678f;
        float rightGain = 0.70710678f;
        float cutoffHz = 20000.0f;
        float frequencyHz = 440.0f;
        float filterState = 0.0f;
        float filterCoefficient = 1.0f;
        float lpfHz = 0.0f;
        float lpr = 1.0f;
        float hpfHz = 0.0f;
        float hpr = 1.0f;
        float bpfHz = 0.0f;
        float bpr = 1.0f;
        float bpnoise = 0.0f;
        float swell = 0.0f;
        float modulatedBpfHz = 0.0f;
        float modulatedBpr = 1.0f;
        float swellCutoffHz = 0.0f;
        ModulationNoiseState bpfNoise;
        ModulationNoiseState bprNoise;
        BiquadState lpfState;
        BiquadState hpfState;
        BiquadState bpfState;
        BiquadState swellState;
        bool mousePan = false;
        bool mouseCutoff = false;
        double attackSeconds = kAttackSeconds;
        double releaseSeconds = kReleaseSeconds;
        std::uint64_t startSample = 0;
        std::uint64_t sustainEndSample = 0;
        std::uint64_t endSample = 0;
        bool foxDotNativeEnvelope = false;
        bool releasing = false;
        bool reverbTail = false;
        std::uint64_t releaseStartSample = 0;
        std::uint64_t releaseEndSample = 0;
        float releaseStartGain = 0.0f;
        bool triggerControlPendingReset = false;
    };

    struct ScheduledAudioNote {
        int playerIndex = 0;
        int midiNote = -1;
        int velocity = 0;
        int synthIndex = 0;
        float synthRate = 0.0f;
        float amplitude = 1.0f;
        float pan = 0.0f;
        float cutoffHz = 20000.0f;
        float lpfHz = 0.0f;
        float lpr = 1.0f;
        float hpfHz = 0.0f;
        float hpr = 1.0f;
        float bpfHz = 0.0f;
        float bpr = 1.0f;
        bool mousePan = false;
        bool mouseCutoff = false;
        double attackSeconds = kAttackSeconds;
        double releaseSeconds = kReleaseSeconds;
        std::uint64_t startSample = 0;
        std::uint64_t endSample = 0;
        std::uint64_t sustainEndSample = 0;
        bool foxDotNativeEnvelope = false;
        int sampleBuffer = -1;
    };

    static bool parseTimeVarControl(const std::string& token, double minimum,
                                    double maximum, TimeVarControl& result) {
        result = {};
        if (token == "-")
            return true;
        if (token.size() > 256 || token.size() < 5 || token[1] != ':' ||
            (token[0] != 'v' && token[0] != 'l'))
            return false;
        const auto middle = token.find(':', 2);
        if (middle == std::string::npos || middle == 2 || middle + 1 >= token.size())
            return false;
        auto parseList = [](const std::string& text, std::array<double, 8>& values,
                            std::size_t& count, double low, double high, bool positive) {
            std::istringstream parts(text);
            std::string part;
            count = 0;
            while (std::getline(parts, part, ',')) {
                if (part.empty() || count >= values.size())
                    return false;
                try {
                    std::size_t used = 0;
                    const double value = std::stod(part, &used);
                    if (used != part.size() || !std::isfinite(value) ||
                        (positive ? value <= 0.0 || value > high
                                  : value < low || value > high))
                        return false;
                    values[count++] = value;
                } catch (...) {
                    return false;
                }
            }
            return count > 0;
        };
        std::array<double, 8> values{};
        std::array<double, 8> durations{};
        std::size_t valueCount = 0;
        std::size_t durationCount = 0;
        if (!parseList(token.substr(2, middle - 2), values, valueCount,
                       minimum, maximum, false) ||
            !parseList(token.substr(middle + 1), durations, durationCount,
                       0.0, 4096.0, true) ||
            (durationCount != 1 && durationCount != valueCount))
            return false;
        result.active = true;
        result.linear = token[0] == 'l';
        result.count = valueCount;
        result.values = values;
        for (std::size_t i = 0; i < valueCount; ++i)
            result.durations[i] = durations[durationCount == 1 ? 0 : i];
        return true;
    }

    static float evaluateTimeVar(const TimeVarControl& control, double beat,
                                 float fallback) {
        if (!control.active || control.count == 0)
            return fallback;
        double cycle = 0.0;
        for (std::size_t i = 0; i < control.count; ++i)
            cycle += control.durations[i];
        if (cycle <= 0.0)
            return fallback;
        double position = std::fmod(std::max(0.0, beat), cycle);
        for (std::size_t i = 0; i < control.count; ++i) {
            const double duration = control.durations[i];
            if (position < duration || i + 1 == control.count) {
                if (!control.linear || control.count == 1)
                    return static_cast<float>(control.values[i]);
                const double progress = std::clamp(position / duration, 0.0, 1.0);
                const double next = control.values[(i + 1) % control.count];
                return static_cast<float>(control.values[i] +
                                          (next - control.values[i]) * progress);
            }
            position -= duration;
        }
        return static_cast<float>(control.values[control.count - 1]);
    }

    static float evaluateSampleSelection(const TimeVarControl& control, double beat,
                                         float fallback) {
        if (!control.active || control.count == 0)
            return fallback;
        if (!control.linear || control.count == 1)
            return evaluateTimeVar(control, beat, fallback);
        double cycle = 0.0;
        for (std::size_t i = 0; i < control.count; ++i)
            cycle += control.durations[i];
        if (cycle <= 0.0)
            return fallback;
        double position = std::fmod(std::max(0.0, beat), cycle);
        for (std::size_t i = 0; i < control.count; ++i) {
            const double duration = control.durations[i];
            if (position < duration || i + 1 == control.count) {
                const double progress = std::clamp(position / duration, 0.0, 1.0);
                const auto next = (i + 1) % control.count;
                return static_cast<float>(control.values[progress >= 0.5 ? next : i]);
            }
            position -= duration;
        }
        return static_cast<float>(control.values[control.count - 1]);
    }

    static bool validSampleSelectionControl(const TimeVarControl& control) {
        for (std::size_t value = 0; value < control.count; ++value) {
            const double index = control.values[value];
            if (!std::isfinite(index) || index < -1000000.0 || index > 1000000.0 ||
                std::floor(index) != index)
                return false;
        }
        return true;
    }

    static bool validSampleRateControl(const TimeVarControl& control) {
        int rateSign = 0;
        for (std::size_t value = 0; value < control.count; ++value) {
            const double rate = control.values[value];
            if (!std::isfinite(rate) || std::abs(rate) < 0.0625 || std::abs(rate) > 8.0)
                return false;
            const int sign = std::signbit(rate) ? -1 : 1;
            if (rateSign && sign != rateSign && control.linear)
                return false;
            rateSign = sign;
        }
        return true;
    }

    void setEditorStatus(std::string status) {
        std::lock_guard<std::mutex> lock(statusMutex_);
        editorStatus_ = std::move(status);
    }

    void joinScriptThread() {
        LifecycleTrace trace("joinScriptThread", this);
        std::thread thread;
        {
            std::lock_guard<std::mutex> lock(scriptThreadMutex_);
            if (scriptThread_.joinable())
                thread = std::move(scriptThread_);
        }
        if (thread.joinable())
            thread.join();
        scriptRunning_.store(false, std::memory_order_release);
    }

    static void addMidiNoteEvent(IEventList* outputEvents, int32 sampleOffset,
                                 int midiNote, int velocity, bool noteOn, int channel = 0,
                                 int32 noteId = -1) {
        if (!outputEvents)
            return;
        Event event{};
        event.busIndex = 0;
        event.sampleOffset = sampleOffset;
        event.ppqPosition = -1.0;
        event.flags = 0;
        if (noteOn) {
            event.type = Event::kNoteOnEvent;
            event.noteOn.channel = static_cast<int16>(channel);
            event.noteOn.pitch = static_cast<int16>(midiNote);
            event.noteOn.tuning = 0.0f;
            event.noteOn.velocity = static_cast<float>(velocity) / 127.0f;
            event.noteOn.length = 0;
            event.noteOn.noteId = noteId;
        } else {
            event.type = Event::kNoteOffEvent;
            event.noteOff.channel = static_cast<int16>(channel);
            event.noteOff.pitch = static_cast<int16>(midiNote);
            event.noteOff.velocity = static_cast<float>(velocity) / 127.0f;
            event.noteOff.noteId = noteId;
            event.noteOff.tuning = 0.0f;
        }
        outputEvents->addEvent(event);
    }

    void stopPlayerMidiNotes(IEventList* outputEvents, int32 sampleOffset, int playerIndex) {
        if (playerIndex < 0 || playerIndex >= static_cast<int>(kPlayerCount))
            return;
        auto& gates = activeMidiGates_[static_cast<std::size_t>(playerIndex)];
        for (auto& gate : gates) {
            if (gate.emitted)
                addMidiNoteEvent(outputEvents, sampleOffset, gate.midiNote, 0, false,
                                 playerIndex, gate.noteId);
            gate = {};
        }
    }

    void stopActiveMidiNotes(IEventList* outputEvents, int32 sampleOffset) {
        for (std::size_t player = 0; player < kPlayerCount; ++player)
            stopPlayerMidiNotes(outputEvents, sampleOffset, static_cast<int>(player));
    }

    std::uint64_t envelopeSamples(double seconds) const {
        return static_cast<std::uint64_t>(
            std::max(1.0, std::round(sampleRate_ * seconds)));
    }

    static bool usesFoxDotNativeEnvelope(int synthIndex, double attackSeconds,
                                         double releaseSeconds) {
        if (synthIndex == 57)
            return false; // Native PlayBuf one-shots use the host's edge fades.
        if (synthIndex >= 31)
            return true; // These definitions contain retriggerable SC envelopes.
        return (synthIndex == 4 || synthIndex == 5 ||
                (synthIndex >= 22 && synthIndex <= 24)) &&
               std::abs(attackSeconds - kAttackSeconds) < 1.0e-9 &&
               std::abs(releaseSeconds - kReleaseSeconds) < 1.0e-9;
    }

    static double foxDotEnvelopeDurationSeconds(int synthIndex,
                                                double sustainSeconds,
                                                double attackSeconds = 0.01) {
        switch (synthIndex) {
        case 4: return 0.020 + sustainSeconds; // bass Env.perc(0.02, sus)
        case 5: return sustainSeconds;         // varsaw Env(sus/2, sus/2)
        case 22: return 0.010 + sustainSeconds; // charm Env.perc(0.01, sus, curve=0)
        case 23: return sustainSeconds * 1.5;   // spark Line(..., sus * 1.5)
        case 24: return sustainSeconds * 2.0;   // blip XLine(..., sus * 2)
        case 32: return 0.000125 + sustainSeconds * 3;
        case 33: return 0.010 + sustainSeconds * 0.25;
        case 34: return sustainSeconds * 1.25;
        case 35: return sustainSeconds + 0.060;
        case 36: return sustainSeconds + 0.030;
        case 37: return sustainSeconds * 2 + 0.010;
        case 38: return sustainSeconds * 1.2 + attackSeconds;
        case 39: return sustainSeconds * (4.0 / 3.0) + 0.010;
        case 40: return sustainSeconds * 1.5;
        case 41: return sustainSeconds + 0.010;
        case 42: return sustainSeconds * 1.5 + 0.010;
        case 43: case 44: case 45: return sustainSeconds * 6;
        case 46: return sustainSeconds * 2;
        case 47: case 51: case 52: return sustainSeconds + 0.010;
        case 48: return sustainSeconds * 1.4;
        case 50: return sustainSeconds + 0.001;
        case 53: return sustainSeconds + 0.125;
        case 54: return sustainSeconds * 4;
        case 55: return sustainSeconds * 3;
        default: return sustainSeconds;
        }
    }

    float voiceEnvelopeAt(const AudioVoice& voice, std::uint64_t sample) const {
        if (voice.releasing) {
            if (sample < voice.releaseStartSample)
                return voice.releaseStartGain;
            if (sample >= voice.releaseEndSample)
                return 0.0f;
            const auto releaseLength = voice.releaseEndSample - voice.releaseStartSample;
            const double progress = static_cast<double>(sample - voice.releaseStartSample) /
                                    static_cast<double>(releaseLength);
            const float fade = static_cast<float>(0.5 * (1.0 + std::cos(kPi * progress)));
            return voice.releaseStartGain * fade;
        }
        if (voice.midiNote < 0 || sample < voice.startSample ||
            sample >= voice.endSample)
            return 0.0f;

        if (voice.foxDotNativeEnvelope) {
            if (voice.synthIndex >= 31) {
                // The graph owns its envelope. Retain a 5 ms end safety fade
                // for upstream step/ramp envelopes and forced voice truncation.
                const auto remaining = voice.endSample - sample;
                return std::min(1.0f, static_cast<float>(remaining) /
                    static_cast<float>(envelopeSamples(0.005)));
            }
            if (voice.synthIndex == 22) {
                const auto attack = envelopeSamples(0.010);
                const auto releaseLength = voice.endSample -
                    std::min(voice.endSample, voice.startSample + attack);
                const auto attackEnd = voice.startSample + attack;
                if (sample < attackEnd) {
                    const double progress = static_cast<double>(sample - voice.startSample) /
                                            static_cast<double>(attack);
                    return static_cast<float>(progress);
                }
                const double progress = static_cast<double>(sample - attackEnd) /
                    static_cast<double>(std::max<std::uint64_t>(1, releaseLength));
                return static_cast<float>(std::max(0.0, 1.0 - progress));
            }
            if (voice.synthIndex == 4) {
                const auto attackEnd = voice.startSample +
                                       envelopeSamples(0.020);
                if (sample < attackEnd)
                    return static_cast<float>(sample - voice.startSample) /
                           static_cast<float>(std::max<std::uint64_t>(
                               1, attackEnd - voice.startSample));
                const auto releaseLength = voice.sustainEndSample - voice.startSample;
                const float progress = static_cast<float>(sample - attackEnd) /
                                       static_cast<float>(std::max<std::uint64_t>(
                                           1, releaseLength));
                return std::max(0.0f, 1.0f - progress);
            }
            if (voice.synthIndex == 5) {
                const auto duration = voice.endSample - voice.startSample;
                const float progress = static_cast<float>(sample - voice.startSample) /
                                       static_cast<float>(std::max<std::uint64_t>(
                                           1, duration));
                return progress < 0.5f
                    ? progress * 2.0f : std::max(0.0f, (1.0f - progress) * 2.0f);
            }
            if (voice.synthIndex == 23) {
                const auto sustainLength = std::max<std::uint64_t>(
                    1, voice.sustainEndSample - voice.startSample);
                const double time = static_cast<double>(sample - voice.startSample);
                const double fadeInLength = std::max(
                    1.0, static_cast<double>(sustainLength) * 0.033);
                const double fadeOutLength = std::max(
                    1.0, static_cast<double>(voice.endSample - voice.startSample));
                const double fadeIn = 0.01 + 0.99 *
                    std::min(1.0, time / fadeInLength);
                const double fadeOut = std::max(
                    0.0001, 1.0 - 0.9999 * time / fadeOutLength);
                return static_cast<float>(fadeIn * fadeOut);
            }
            if (voice.synthIndex == 24) {
                const double duration = static_cast<double>(
                    std::max<std::uint64_t>(1, voice.endSample - voice.startSample));
                const double progress = std::clamp(
                    static_cast<double>(sample - voice.startSample) / duration,
                    0.0, 1.0);
                return static_cast<float>(std::pow(0.0001, progress));
            }
        }

        const auto duration = voice.endSample - voice.startSample;
        const auto attack = std::min(envelopeSamples(voice.attackSeconds), duration / 2);
        const auto releaseSeconds = std::max(0.005, voice.releaseSeconds);
        const auto release = std::min(envelopeSamples(releaseSeconds), duration / 2);
        float gain = 1.0f;
        if (attack > 0 && sample < voice.startSample + attack) {
            const double progress = static_cast<double>(sample - voice.startSample) /
                                    static_cast<double>(attack);
            const float fadeIn = static_cast<float>(0.5 * (1.0 - std::cos(kPi * progress)));
            gain = std::min(gain, fadeIn);
        }
        if (release > 0 && sample >= voice.endSample - release) {
            const double progress = static_cast<double>(sample - (voice.endSample - release)) /
                                    static_cast<double>(release);
            const float fadeOut = static_cast<float>(0.5 * (1.0 + std::cos(kPi * progress)));
            gain = std::min(gain, fadeOut);
        }
        return gain;
    }

    static float foxDotAmplitude(int synthIndex, float amplitude) {
        if (synthIndex >= 31)
            return 1.0f; // amp arithmetic is inside the original SC graph.
        if (synthIndex == 4)
            return 0.8f * amplitude * amplitude;
        if (synthIndex == 5 || synthIndex == 22)
            return amplitude * amplitude;
        if (synthIndex == 12)
            return amplitude * amplitude;
        if (synthIndex == 23 || synthIndex == 24)
            return 0.3f * amplitude;
        if (synthIndex == 25)
            return amplitude * 0.125f;
        if (synthIndex == 27)
            return amplitude * 0.1f;
        if (synthIndex == 28)
            return amplitude / 6.0f;
        if (synthIndex == 29)
            return amplitude * 0.2f;
        if (synthIndex == 30)
            return amplitude / 6.0f;
        return amplitude;
    }

    float safeSynthFrequency(int synthIndex, int midiNote) const {
        const float requested = 440.0f * std::pow(
            2.0f, static_cast<float>(midiNote - 69) / 12.0f);
        float highestPartial = 1.0f;
        if (synthIndex == 5)
            highestPartial = 1.005f;
        else if (synthIndex == 22)
            highestPartial = 16.05f;
        else if (synthIndex == 23)
            highestPartial = 1.002f;
        else if (synthIndex == 24)
            highestPartial = 2.005f;
        else if (synthIndex == 25 || synthIndex == 29)
            highestPartial = 1.01f;
        else if (synthIndex == 27)
            highestPartial = 1.02f;
        else if (synthIndex == 28)
            highestPartial = 1.5f;
        const float safeLimit = static_cast<float>(sampleRate_ * 0.45) /
                                highestPartial;
        return std::min(requested, safeLimit);
    }

    void deactivateAudioVoice(AudioVoice& voice) {
        if (voice.activeSynth)
            Node_SetRun(&voice.activeSynth->mNode, 0);
        const auto synthGraphs = voice.synthGraphs;
        voice = {};
        voice.synthGraphs = synthGraphs;
    }

    void beginReverbTail(AudioVoice& voice, std::uint64_t tailStartSample) {
        voice.midiNote = -1;
        voice.releasing = true;
        voice.reverbTail = true;
        voice.releaseStartSample = tailStartSample;
        voice.releaseEndSample =
            tailStartSample + envelopeSamples(kReverbSendReleaseSeconds);
        voice.releaseStartGain = 0.0f;
    }

    static Graph* graphForSynthIndex(AudioVoice& voice, int synthIndex) {
        if (synthIndex < 0 ||
            static_cast<std::size_t>(synthIndex) >= kSynthIndexToGraphIndex.size())
            return nullptr;
        const auto graphIndex =
            kSynthIndexToGraphIndex[static_cast<std::size_t>(synthIndex)];
        return graphIndex < voice.synthGraphs.size()
            ? voice.synthGraphs[graphIndex]
            : nullptr;
    }

    Graph* createSynthGraph(std::size_t voiceIndex, std::size_t graphIndex,
                            std::string& error, bool startImmediately = false) {
        if (!world_ || !rootGroup_ || graphIndex >= synthDefinitions_.size() ||
            !synthDefinitions_[graphIndex]) {
            error = "The SuperCollider world or selected synth definition is not ready.";
            return nullptr;
        }
        if (voiceIndex >= audioVoices_.size()) {
            error = "The selected synth voice is outside the supported range.";
            return nullptr;
        }

        auto& voice = audioVoices_[voiceIndex];
        auto*& graph = voice.synthGraphs[graphIndex];
        if (graph)
            return graph;

        const auto cachedCount = static_cast<std::size_t>(std::count_if(
            voice.synthGraphs.begin(), voice.synthGraphs.end(),
            [](const Graph* cached) { return cached != nullptr; }));
        if (cachedCount >= kMaxCachedSynthGraphsPerVoice) {
            const auto evict = std::find_if(
                voice.synthGraphs.begin(), voice.synthGraphs.end(),
                [&](Graph* cached) { return cached && cached != voice.activeSynth; });
            if (evict == voice.synthGraphs.end()) {
                error = "No inactive synth graph is available for replacement.";
                return nullptr;
            }
            Node_SetRun(&(*evict)->mNode, 0);
            Node_End(&(*evict)->mNode);
            *evict = nullptr;
        }

        sc_msg_iter noControls(0, "");
        Graph* created = nullptr;
        const auto graphError =
            Graph_New(world_, synthDefinitions_[graphIndex], nextDynamicGraphNodeId_++,
                      &noControls, &created, true);
        if (graphError != 0 || !created) {
            error = "SuperCollider could not create synth graph " +
                    std::string(kSynthGraphNames[graphIndex]) + " (error " +
                    std::to_string(static_cast<int>(graphError)) + ").";
            return nullptr;
        }

        const auto outputBus = kFirstAudioBus +
                               kAudioBusStride * static_cast<int>(voiceIndex);
        Graph_SetControl(created, 0, static_cast<float>(outputBus));
        Graph_SetControl(created, 6, static_cast<float>(kReverbBaseBus));
        Graph_SetControl(created, 7, 0.0f);
        Graph_SetControl(created, 8, 20000.0f);
        if (graphIndex >= 12)
            Graph_SetControl(created, 9, 0.0f);
        if (graphIndex == 1)
            Graph_SetControl(created, 2, 8.0f);
        else if (graphIndex == 2)
            Graph_SetControl(created, 2, 0.0f);
        Group_AddTail(rootGroup_, &created->mNode);
        // SC cannot resume Graph_NullFirstCalc until it has had a world tick.
        // A fresh note must keep Graph_FirstCalc until its first render.
        if (!startImmediately)
            Node_SetRun(&created->mNode, 0);
        graph = created;
        return created;
    }

    struct CachedSample {
        std::filesystem::path path;
        PortableSamples::Wave wave;
        int buffer = -1;
    };

    bool prepareSampleBuffers(std::array<PatternState, kPlayerCount>& updates,
                              std::uint8_t updateMask, std::string& error) {
        bool needed = false;
        for (std::size_t player = 0; player < kPlayerCount; ++player)
            needed |= (updateMask & (1u << player)) && updates[player].active &&
                      updates[player].synthIndex == 57 && updates[player].eventCount != 0;
        if (!needed) return true;
        if (!initializeWorld()) {
            std::lock_guard<std::mutex> lock(worldAccessMutex_);
            error = worldInitializationError_; return false;
        }
        std::uint64_t generation;
        {
            std::lock_guard<std::mutex> lock(worldAccessMutex_);
            generation = sampleWorldGeneration_;
        }
        const auto root = pluginBinaryPath().parent_path().parent_path().parent_path() /
                          "Portable" / "Samples";
        setEditorStatus("Loading portable WAV samples...");
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            auto& update = updates[player];
            if (!(updateMask & (1u << player)) || !update.active || update.synthIndex != 57)
                continue;
            update.sampleWorld = generation;
            for (std::size_t index = 0; index < update.eventCount; ++index) {
                auto& event = update.events[index];
                event.sampleVariantCount = update.sampleControl.active
                    ? update.sampleControl.count : 1;
                if (event.sampleVariantCount == 0 ||
                    event.sampleVariantCount > event.sampleVariantBuffers.size()) {
                    error = "Sample var/linvar has an unsupported number of variants.";
                    return false;
                }
                event.sampleVariantBuffers.fill(-1);
                event.sampleVariantDurations.fill(0.0);
                for (std::size_t variant = 0; variant < event.sampleVariantCount; ++variant) {
                    const auto rawIndex = update.sampleControl.active
                        ? update.sampleControl.values[variant] : event.sampleIndex;
                    if (!std::isfinite(rawIndex) || std::floor(rawIndex) != rawIndex ||
                        rawIndex < -1000000.0 || rawIndex > 1000000.0) {
                        error = "Sample var/linvar requires whole sample indexes from -1000000 to 1000000.";
                        return false;
                    }
                    std::filesystem::path path;
                    if (!PortableSamples::resolveWave(root, event.sampleSymbol,
                                                       static_cast<int>(rawIndex), path, error))
                        return false;
                    auto ready = [&]() {
                        if (world_ && generation == sampleWorldGeneration_) return true;
                        error = "The audio engine restarted while loading samples. Press Run to retry.";
                        return false;
                    };
                    CachedSample* cached = nullptr;
                    std::size_t decodeBudget = 0;
                    {
                        std::lock_guard<std::mutex> lock(worldAccessMutex_);
                        if (!ready()) return false;
                        for (const auto& item : sampleCache_)
                            if (item->path == path) { cached = item.get(); break; }
                        if (!cached) {
                            decodeBudget = PortableSamples::kCacheBudgetBytes - sampleCacheBytes_;
                            if (decodeBudget < 16) {
                                error = "The 128 MiB decoded sample cache is full. Use shorter WAVs or reopen the plugin.";
                                return false;
                            }
                        }
                    }
                    if (!cached) {
                        // Expensive I/O/decoding is outside the world/audio lock.
                        auto loaded = std::make_unique<CachedSample>();
                        loaded->path = path;
                        if (!PortableSamples::readWave(path, loaded->wave, error, decodeBudget))
                            return false;
                        std::lock_guard<std::mutex> lock(worldAccessMutex_);
                        if (!ready()) return false;
                        const auto bytes = loaded->wave.mono.size() * sizeof(float);
                        if (bytes > PortableSamples::kCacheBudgetBytes - sampleCacheBytes_) {
                            error = "The 128 MiB decoded sample cache is full. Use shorter WAVs or reopen the plugin.";
                            return false;
                        }
                        // Buffer zero remains the nil buffer, as in FoxDot.
                        if (sampleCache_.size() + 1 >= world_->mNumSndBufs) {
                            error = "SuperCollider sample buffers are full. Reopen the plugin to clear the cache.";
                            return false;
                        }
                        loaded->buffer = static_cast<int>(sampleCache_.size() + 1);
                        cached = loaded.get();
                        // Publish ownership before either SC buffer mirror points at the vector.
                        sampleCache_.push_back(std::move(loaded));
                        SndBuf buffer{};
                        buffer.data = cached->wave.mono.data();
                        buffer.frames = cached->wave.frames;
                        buffer.samples = buffer.frames;
                        buffer.channels = 1;
                        buffer.samplerate = cached->wave.sampleRate;
                        buffer.sampledur = 1.0 / buffer.samplerate;
                        world_->mSndBufs[cached->buffer] = buffer;
                        world_->mSndBufsNonRealTimeMirror[cached->buffer] = buffer;
                        ++world_->mSndBufUpdates[cached->buffer].writes;
                        sampleCacheBytes_ += bytes;
                    }
                    event.sampleVariantBuffers[variant] = cached->buffer;
                    event.sampleVariantDurations[variant] =
                        cached->wave.frames / cached->wave.sampleRate;
                }
                event.sampleBuffer = event.sampleVariantBuffers[0];
                event.sampleDurationSeconds = event.sampleVariantDurations[0];
            }
        }
        return true;
    }

    void detachSampleBuffers() {
        // World_Cleanup uses free_alig(). Our immutable audio vectors are
        // plugin-owned: detach both SC mirrors before cleanup, then destroy the
        // vectors only after the graph/unit destructors have finished.
        for (const auto& cached : sampleCache_) {
            if (!world_ || cached->buffer < 0 ||
                static_cast<unsigned>(cached->buffer) >= world_->mNumSndBufs) continue;
            for (auto* buffers : {world_->mSndBufs, world_->mSndBufsNonRealTimeMirror})
                if (buffers[cached->buffer].data == cached->wave.mono.data())
                    buffers[cached->buffer] = {};
        }
    }

    bool prepareSynthGraphs(
        const std::array<PatternState, kPlayerCount>& updates,
        std::uint8_t updateMask, std::string& error) {
        // Run may follow a failed host setup (e.g. a previously missing UGen
        // folder). Retry once on this worker, never on the audio callback.
        if (!initializeWorld()) {
            std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
            error = worldInitializationError_;
            return false;
        }
        std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
        if (!world_) {
            error = "The audio engine changed during Run. Enable host audio processing and press Run again.";
            return false;
        }

        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            if ((updateMask & static_cast<std::uint8_t>(1u << player)) == 0 ||
                !updates[player].active)
                continue;
            const auto synthIndex = updates[player].synthIndex;
            if (synthIndex < 0 ||
                static_cast<std::size_t>(synthIndex) >= kSynthIndexToGraphIndex.size()) {
                error = "The selected synth index is outside the registered synth bank.";
                return false;
            }
            // New-bank notes are born running, with controls configured before
            // World_Run. Do not create paused, never-rendered templates for them.
            if (synthIndex >= 31)
                continue;
            const auto firstVoice = player * kVoiceSlotsPerPlayer;
            const auto graphIndex =
                kSynthIndexToGraphIndex[static_cast<std::size_t>(synthIndex)];
            for (std::size_t offset = 0; offset < kVoiceSlotsPerPlayer; ++offset) {
                if (!createSynthGraph(firstVoice + offset, graphIndex, error))
                    return false;
            }
        }
        return true;
    }

    void stopAudioVoices(int playerIndex = -1) {
        const auto now = sampleClock_ +
                         static_cast<std::uint64_t>(kSCBlockSize - cursor_);
        for (auto& voice : audioVoices_) {
            if ((playerIndex >= 0 && voice.playerIndex != playerIndex) ||
                voice.midiNote < 0 || voice.releasing)
                continue;
            const float gain = voiceEnvelopeAt(voice, now);
            if (gain <= 0.0f) {
                beginReverbTail(voice, now);
                continue;
            }
            voice.releasing = true;
            voice.releaseStartSample = now;
            voice.releaseEndSample = now + envelopeSamples(std::max(0.005, voice.releaseSeconds));
            voice.releaseStartGain = gain;
            voice.midiNote = -1;
        }
    }

    void collectMidiEvents(std::uint64_t blockStart, std::uint64_t blockEnd,
                           std::array<TimedMidiEvent, kMaxMidiEventsPerBlock>& result,
                           std::size_t& resultCount) const {
        resultCount = 0;
        auto append = [&](std::uint64_t sample, int32 noteId, int playerIndex, int note,
                          int velocity, bool noteOn) {
            if (sample < blockStart || sample >= blockEnd || resultCount >= result.size())
                return;
            result[resultCount++] = TimedMidiEvent{
                sample, noteId, playerIndex, note, velocity, noteOn};
        };

        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            const auto& pattern = activePatterns_[player];
            if (!pattern.active || pattern.eventCount == 0 ||
                pattern.lengthBeats <= 0.0 || pattern.bpm <= 0.0)
                continue;
            const auto patternStart = patternStartSamples_[player];
            if (blockEnd <= patternStart)
                continue;
            const double samplesPerBeat = sampleRate_ * 60.0 / pattern.bpm;
            const double startBeat = blockStart <= patternStart ? 0.0
                : static_cast<double>(blockStart - patternStart) / samplesPerBeat;
            const double endBeat = static_cast<double>(blockEnd - patternStart) / samplesPerBeat;
            for (std::size_t i = 0; i < pattern.eventCount; ++i) {
                const auto& note = pattern.events[i];
                const auto firstCycle = std::max<std::int64_t>(
                    0, static_cast<std::int64_t>(std::floor(
                           (startBeat - note.beat - 15.0 - note.durationBeats) /
                           pattern.lengthBeats)) - 1);
                const auto lastCycle = static_cast<std::int64_t>(
                    std::ceil((endBeat - note.beat) / pattern.lengthBeats)) + 1;
                for (auto cycle = firstCycle; cycle <= lastCycle; ++cycle) {
                    const double baseEventBeat =
                        static_cast<double>(cycle) * pattern.lengthBeats + note.beat;
                    const double dynamicDelay = evaluateTimeVar(pattern.noteDelayControl,
                                                                 baseEventBeat, 0.0f);
                    const double onsetBeat = baseEventBeat + note.delayBeats + dynamicDelay;
                    const double offsetBeat = onsetBeat + note.durationBeats;
                    const auto onSample = patternStart + static_cast<std::uint64_t>(
                        std::llround(onsetBeat * samplesPerBeat));
                    const auto offSample = patternStart + static_cast<std::uint64_t>(
                        std::llround(offsetBeat * samplesPerBeat));
                    const auto noteId = static_cast<int32>(
                        ((player * kMaxPatternEvents + i) << 16) |
                        (static_cast<std::uint64_t>(cycle) & 0xffffu));
                    append(onSample, noteId, static_cast<int>(player),
                           note.midiNote, note.velocity, true);
                    append(offSample, noteId, static_cast<int>(player),
                           note.midiNote, 0, false);
                }
            }
        }

        std::sort(result.begin(), result.begin() + resultCount,
                  [](const TimedMidiEvent& a, const TimedMidiEvent& b) {
                      if (a.sample != b.sample) return a.sample < b.sample;
                      if (a.noteOn != b.noteOn) return !a.noteOn;
                      return a.playerIndex < b.playerIndex;
                  });
    }

    void emitPatternMidi(ProcessData& data) {
        std::array<TimedMidiEvent, kMaxMidiEventsPerBlock> events{};
        std::size_t count = 0;
        collectMidiEvents(sampleClock_, sampleClock_ + static_cast<std::uint64_t>(data.numSamples),
                          events, count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            const auto player = static_cast<std::size_t>(event.playerIndex);
            if (event.noteOn) {
                auto& gates = activeMidiGates_[player];
                auto gate = std::find_if(gates.begin(), gates.end(),
                                         [](const ActiveMidiGate& item) {
                                             return item.noteId < 0;
                                         });
                if (gate == gates.end())
                    continue;
                gate->noteId = event.noteId;
                gate->midiNote = event.midiNote;
                gate->emitted = !mutedPlayers_[player];
                if (gate->emitted) {
                    addMidiNoteEvent(data.outputEvents,
                                     static_cast<int32>(event.sample - sampleClock_),
                                     event.midiNote, event.velocity, true,
                                     event.playerIndex, event.noteId);
                }
            } else {
                auto& gates = activeMidiGates_[player];
                auto gate = std::find_if(gates.begin(), gates.end(),
                                         [&](const ActiveMidiGate& item) {
                                             return item.noteId == event.noteId &&
                                                    item.midiNote == event.midiNote;
                                         });
                if (gate == gates.end())
                    continue;
                if (gate->emitted)
                    addMidiNoteEvent(data.outputEvents,
                                     static_cast<int32>(event.sample - sampleClock_),
                                     event.midiNote, 0, false,
                                     event.playerIndex, event.noteId);
                *gate = {};
            }
        }
    }

    void updateSynthForBlock(std::uint64_t blockStart) {
        std::array<ScheduledAudioNote, kAudioVoiceCount> desired{};
        std::size_t desiredCount = 0;
        const auto midpoint = blockStart + kSCBlockSize / 2;
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            const auto& pattern = activePatterns_[player];
            const auto patternStart = patternStartSamples_[player];
            if (!pattern.active || pattern.eventCount == 0 ||
                pattern.lengthBeats <= 0.0 || pattern.bpm <= 0.0 ||
                midpoint < patternStart)
                continue;
            const double samplesPerBeat = sampleRate_ * 60.0 / pattern.bpm;
            const double beat = static_cast<double>(midpoint - patternStart) / samplesPerBeat;
            std::size_t playerVoices = 0;
            for (std::size_t i = 0; i < pattern.eventCount; ++i) {
                const auto& event = pattern.events[i];
                const bool isSample = pattern.synthIndex == 57;
                if (isSample && (event.sampleBuffer < 1 ||
                                 pattern.sampleWorld != sampleWorldGeneration_))
                    continue;
                double longestSampleSeconds = 0.0;
                double minimumSampleRate = std::abs(event.sampleRate);
                if (isSample) {
                    for (std::size_t variant = 0; variant < event.sampleVariantCount; ++variant)
                        longestSampleSeconds = std::max(longestSampleSeconds,
                                                        event.sampleVariantDurations[variant]);
                    if (pattern.sampleRateControl.active)
                        for (std::size_t value = 0; value < pattern.sampleRateControl.count; ++value)
                            minimumSampleRate = std::min(
                                minimumSampleRate,
                                std::abs(pattern.sampleRateControl.values[value]));
                }
                double sampleSeconds = isSample && minimumSampleRate > 0.0
                    ? longestSampleSeconds / minimumSampleRate : 0.0;
                if (isSample && event.sampleGate)
                    sampleSeconds = std::min(sampleSeconds, event.durationBeats * 60.0 / pattern.bpm);
                const double lookback = isSample ? sampleSeconds * pattern.bpm / 60.0 : event.durationBeats;
                if (playerVoices >= kMaxPolyphony || desiredCount >= desired.size())
                    break;
                const auto firstCycle = std::max<std::int64_t>(
                    0, static_cast<std::int64_t>(std::floor(
                           (beat - event.beat - 15.0 - lookback) /
                           pattern.lengthBeats)) - 1);
                const auto lastCycle = static_cast<std::int64_t>(
                    std::floor((beat - event.beat) / pattern.lengthBeats));
                for (auto cycle = firstCycle; cycle <= lastCycle; ++cycle) {
                    const double baseEventBeat =
                        static_cast<double>(cycle) * pattern.lengthBeats + event.beat;
                    const double dynamicDelay = evaluateTimeVar(pattern.noteDelayControl,
                                                                 baseEventBeat, 0.0f);
                    const double absoluteStartBeat = baseEventBeat + event.delayBeats + dynamicDelay;
                    std::size_t selectedVariant = 0;
                    int selectedSampleBuffer = event.sampleBuffer;
                    double selectedSampleRate = event.sampleRate;
                    double selectedSampleSeconds = sampleSeconds;
                    if (isSample) {
                        const double selection = evaluateSampleSelection(
                            pattern.sampleControl, absoluteStartBeat,
                            static_cast<float>(event.sampleIndex));
                        if (pattern.sampleControl.active) {
                            for (std::size_t variant = 0;
                                 variant < event.sampleVariantCount; ++variant) {
                                if (pattern.sampleControl.values[variant] == selection) {
                                    selectedVariant = variant;
                                    break;
                                }
                            }
                        }
                        if (selectedVariant >= event.sampleVariantCount)
                            continue;
                        selectedSampleBuffer = event.sampleVariantBuffers[selectedVariant];
                        selectedSampleRate = pattern.sampleRateControl.active
                            ? evaluateTimeVar(pattern.sampleRateControl, absoluteStartBeat,
                                              static_cast<float>(event.sampleRate))
                            : event.sampleRate;
                        if (selectedSampleBuffer < 1 ||
                            std::abs(selectedSampleRate) < 0.0625 ||
                            std::abs(selectedSampleRate) > 8.0)
                            continue;
                        selectedSampleSeconds =
                            event.sampleVariantDurations[selectedVariant] /
                            std::abs(selectedSampleRate);
                        if (event.sampleGate)
                            selectedSampleSeconds = std::min(
                                selectedSampleSeconds,
                                event.durationBeats * 60.0 / pattern.bpm);
                    }
                    const double sustainEndBeat = absoluteStartBeat + event.durationBeats;
                    const bool foxDotNativeEnvelope = usesFoxDotNativeEnvelope(
                        pattern.synthIndex, event.attackSeconds, event.releaseSeconds);
                    const double sustainSeconds =
                        event.durationBeats * 60.0 / pattern.bpm;
                    // File-based upstream patches except sinepad do not apply blur.
                    const bool appliesBlur = pattern.synthIndex >= 31 &&
                        !(pattern.synthIndex >= 35 && pattern.synthIndex <= 40) &&
                        pattern.synthIndex != 42 && pattern.synthIndex != 48;
                    const double envelopeSustain = sustainSeconds *
                        (appliesBlur ? pattern.synthControls[6] : 1.0);
                    const double sourceEnvelopeSeconds = isSample ? selectedSampleSeconds : foxDotNativeEnvelope
                        ? foxDotEnvelopeDurationSeconds(pattern.synthIndex, envelopeSustain,
                                                        event.attackSeconds)
                        : sustainSeconds;
                    const double absoluteEndBeat = absoluteStartBeat +
                        sourceEnvelopeSeconds * pattern.bpm / 60.0;
                    if (beat < absoluteStartBeat || beat >= absoluteEndBeat)
                        continue;
                    if (playerVoices >= kMaxPolyphony || desiredCount >= desired.size())
                        break;
                    desired[desiredCount++] = ScheduledAudioNote{
                        static_cast<int>(player), event.midiNote, event.velocity,
                        pattern.synthIndex, isSample ? static_cast<float>(selectedSampleRate) : pattern.synthRate,
                        pattern.amplitude, pattern.pan, pattern.cutoffHz,
                        pattern.lpfHz, pattern.lpr, pattern.hpfHz, pattern.hpr, pattern.bpfHz, pattern.bpr,
                        pattern.mousePan, pattern.mouseCutoff,
                        event.attackSeconds, event.releaseSeconds,
                        patternStart + static_cast<std::uint64_t>(
                            std::llround(absoluteStartBeat * samplesPerBeat)),
                        patternStart + static_cast<std::uint64_t>(
                            std::llround(absoluteEndBeat * samplesPerBeat)),
                        patternStart + static_cast<std::uint64_t>(
                            std::llround(sustainEndBeat * samplesPerBeat)),
                        foxDotNativeEnvelope, selectedSampleBuffer};
                    ++playerVoices;
                }
            }
        }

        for (auto& voice : audioVoices_) {
            if (voice.reverbTail && voice.releaseEndSample <= blockStart)
                deactivateAudioVoice(voice);
            else if (!voice.releasing && voice.midiNote >= 0 &&
                     voice.endSample <= blockStart)
                beginReverbTail(voice, blockStart);
            else if (voice.releasing && !voice.reverbTail &&
                     voice.releaseEndSample <= blockStart)
                beginReverbTail(voice, blockStart);
        }

        for (std::size_t i = 0; i < desiredCount; ++i) {
            const auto& note = desired[i];
            const auto playerStart =
                static_cast<std::size_t>(note.playerIndex) * kVoiceSlotsPerPlayer;
            auto voiceBegin = audioVoices_.begin() +
                              static_cast<std::ptrdiff_t>(playerStart);
            auto voiceEnd = voiceBegin +
                            static_cast<std::ptrdiff_t>(kVoiceSlotsPerPlayer);
            auto found = std::find_if(voiceBegin, voiceEnd,
                                      [&](const AudioVoice& voice) {
                                          return voice.playerIndex == note.playerIndex &&
                                                 voice.midiNote == note.midiNote &&
                                                 voice.startSample == note.startSample;
                                      });
            if (found == voiceEnd) {
                found = std::find_if(voiceBegin, voiceEnd,
                                     [](const AudioVoice& voice) {
                                         return voice.midiNote < 0 && !voice.releasing;
                                     });
                if (found == voiceEnd)
                    continue;
                found->playerIndex = note.playerIndex;
                found->midiNote = note.midiNote;
                found->velocity = note.velocity;
                found->synthIndex = note.synthIndex;
                found->amplitude = foxDotAmplitude(note.synthIndex, note.amplitude);
                found->mousePan = note.mousePan;
                found->mouseCutoff = note.mouseCutoff;
                found->attackSeconds = note.attackSeconds;
                found->releaseSeconds = note.releaseSeconds;
                found->pan = note.mousePan ? 0.0f : note.pan;
                found->leftGain = std::sqrt(std::max(0.0f, (1.0f - found->pan) * 0.5f));
                found->rightGain = std::sqrt(std::max(0.0f, (1.0f + found->pan) * 0.5f));
                found->cutoffHz = note.cutoffHz;
                found->lpfHz = static_cast<float>(note.lpfHz);
                found->lpr = static_cast<float>(note.lpr);
                found->hpfHz = static_cast<float>(note.hpfHz);
                found->hpr = static_cast<float>(note.hpr);
                found->bpfHz = static_cast<float>(note.bpfHz);
                found->bpr = static_cast<float>(note.bpr);
                const auto playerIndex = static_cast<std::size_t>(note.playerIndex);
                found->bpnoise = activePatterns_[playerIndex].bpnoise;
                found->swell = activePatterns_[playerIndex].swell;
                found->modulatedBpfHz = found->bpfHz;
                found->modulatedBpr = found->bpr;
                found->swellCutoffHz = 0.0f;
                const auto seed = static_cast<std::uint32_t>(
                    (note.startSample ^ (static_cast<std::uint64_t>(note.midiNote) << 16) ^
                     (static_cast<std::uint64_t>(note.playerIndex + 1) << 24)) &
                    0xffffffffu);
                seedNoiseState(found->bpfNoise, seed ^ 0x9e3779b9u);
                seedNoiseState(found->bprNoise, seed ^ 0x85ebca6bu);
                found->filterState = 0.0f;
                found->lpfState = {};
                found->hpfState = {};
                found->bpfState = {};
                found->swellState = {};
                const double maxCutoff = sampleRate_ * 0.45;
                const double clampedCutoff = std::min<double>(note.cutoffHz, maxCutoff);
                found->filterCoefficient = note.synthIndex >= 31 || clampedCutoff >= maxCutoff
                    ? 1.0f
                    : static_cast<float>(1.0 - std::exp(-2.0 * kPi * clampedCutoff / sampleRate_));
                found->frequencyHz = safeSynthFrequency(note.synthIndex,
                                                        note.midiNote);
                found->startSample = note.startSample;
                found->sustainEndSample = note.sustainEndSample;
                found->endSample = note.endSample;
                found->foxDotNativeEnvelope = note.foxDotNativeEnvelope;
                found->releasing = false;
                found->reverbTail = false;
                auto* selectedSynth = graphForSynthIndex(*found, note.synthIndex);
                if (note.synthIndex >= 31) {
                    // Original FoxDot starts a fresh SC node for each note.
                    // Reusing oscillator state is observably wrong for e.g.
                    // pads (zero-Hz Pulse state), and impulse-driven patches.
                    // Graph_New/Node_End use SC's bounded RT memory pool.
                    const auto graphIndex = kSynthIndexToGraphIndex[
                        static_cast<std::size_t>(note.synthIndex)];
                    if (selectedSynth) {
                        if (found->activeSynth == selectedSynth)
                            found->activeSynth = nullptr;
                        Node_SetRun(&selectedSynth->mNode, 0);
                        Node_End(&selectedSynth->mNode);
                        found->synthGraphs[graphIndex] = nullptr;
                    }
                    std::string graphError;
                    selectedSynth = createSynthGraph(
                        static_cast<std::size_t>(std::distance(audioVoices_.begin(), found)),
                        graphIndex, graphError, true);
                    if (!selectedSynth) {
                        found->midiNote = -1;
                        found->velocity = 0;
                        continue;
                    }
                }
                if (found->activeSynth && found->activeSynth != selectedSynth)
                    Node_SetRun(&found->activeSynth->mNode, 0);
                found->activeSynth = selectedSynth;
                if (found->activeSynth) {
                    Graph_SetControl(found->activeSynth, 1, found->frequencyHz);
                    const auto playerBus = kReverbBaseBus + note.playerIndex * kReverbBusStride;
                    Graph_SetControl(found->activeSynth, 6, static_cast<float>(playerBus));
                    Graph_SetControl(found->activeSynth, 7, reverbEnabled(activePatterns_[static_cast<std::size_t>(note.playerIndex)]) ? 1.0f : 0.0f);
                    Graph_SetControl(found->activeSynth, 8, found->cutoffHz);
                    if (note.synthIndex <= 3) {
                        for (int mode = 0; mode < 4; ++mode)
                            Graph_SetControl(found->activeSynth, 2 + mode,
                                         note.synthIndex == mode ? 1.0f : 0.0f);
                    } else {
                        Graph_SetControl(found->activeSynth, 2, note.synthRate);
                        if (note.synthIndex == 57)
                            Graph_SetControl(found->activeSynth, 22, static_cast<float>(note.sampleBuffer));
                        if (note.synthIndex == 10) {
                            const auto sustainSamples = note.endSample - note.startSample;
                            Graph_SetControl(found->activeSynth, 3,
                                static_cast<float>(static_cast<double>(sustainSamples) /
                                                   sampleRate_));
                        }
                        if (note.synthIndex >= 22 && note.synthIndex <= 30) {
                            const auto sustainSamples =
                                note.sustainEndSample - note.startSample;
                            Graph_SetControl(found->activeSynth, 3,
                                static_cast<float>(static_cast<double>(sustainSamples) /
                                                   sampleRate_));
                        }
                        if (note.synthIndex >= 15 && note.synthIndex <= 21) {
                            const auto sustainSamples = note.endSample - note.startSample;
                            Graph_SetControl(found->activeSynth, 3,
                                static_cast<float>(static_cast<double>(sustainSamples) /
                                                   sampleRate_));
                            Graph_SetControl(found->activeSynth, 9, 1.0f);
                            found->triggerControlPendingReset = true;
                        }
                        if (note.synthIndex >= 31) {
                            Graph_SetControl(found->activeSynth, 3,
                                static_cast<float>(static_cast<double>(
                                    note.sustainEndSample - note.startSample) / sampleRate_));
                            const auto& state = activePatterns_[static_cast<std::size_t>(note.playerIndex)];
                            for (std::size_t control = 0; control < state.synthControls.size(); ++control)
                                Graph_SetControl(found->activeSynth, static_cast<int>(11 + control),
                                                static_cast<float>(state.synthControls[control]));
                            Graph_SetControl(found->activeSynth, 10, note.amplitude);
                            Graph_SetControl(found->activeSynth, 18, note.cutoffHz);
                            Graph_SetControl(found->activeSynth, 19, static_cast<float>(note.attackSeconds));
                            Graph_SetControl(found->activeSynth, 20, static_cast<float>(note.releaseSeconds));
                            Graph_SetControl(found->activeSynth, 21,
                                static_cast<float>(60.0 / state.bpm));
                            Graph_SetControl(found->activeSynth, 9, 1.0f);
                            found->triggerControlPendingReset = true;
                        }
                    }
                    Node_SetRun(&found->activeSynth->mNode, 1);
                }
            }
            found->velocity = note.velocity;
        }
    }

    static float randomBipolar(ModulationNoiseState& state) {
        auto value = state.randomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state.randomState = value == 0 ? 1u : value;
        return static_cast<float>(state.randomState >> 8) *
                   (2.0f / 16777215.0f) -
               1.0f;
    }

    static void seedNoiseState(ModulationNoiseState& state, std::uint32_t seed) {
        state = {};
        state.randomState = seed == 0 ? 1u : seed;
        state.from = randomBipolar(state);
        state.to = randomBipolar(state);
    }

    float advanceNoise1(ModulationNoiseState& state, double rateHz) const {
        if (!(rateHz > 0.0) || !(sampleRate_ > 0.0))
            return 0.0f;
        state.phase += std::min(rateHz, sampleRate_ * 0.25) *
                       static_cast<double>(kSCBlockSize) / sampleRate_;
        while (state.phase >= 1.0) {
            state.phase -= 1.0;
            state.from = state.to;
            state.to = randomBipolar(state);
        }
        return state.from + (state.to - state.from) *
                                static_cast<float>(state.phase);
    }

    float swellEnvelopeAt(const AudioVoice& voice, std::uint64_t sample) const {
        if (sample < voice.startSample || sample >= voice.endSample)
            return 0.0f;
        const auto duration = voice.endSample - voice.startSample;
        if (duration == 0)
            return 0.0f;
        const double progress = static_cast<double>(sample - voice.startSample) /
                               static_cast<double>(duration);
        if (progress >= 0.5)
            return 0.0f;
        if (progress < 0.25) {
            const double rise = progress / 0.25;
            return static_cast<float>(0.5 * (1.0 - std::cos(kPi * rise)));
        }
        const double fall = (progress - 0.25) / 0.25;
        return static_cast<float>(0.5 * (1.0 + std::cos(kPi * fall)));
    }

    void configureBiquad(BiquadState& filter, float cutoffHz, float rq, int mode) {
        const bool enabled = cutoffHz > 0.0f;
        if (!enabled) {
            filter.enabled = false;
            return;
        }
        const float maxHz = static_cast<float>(sampleRate_ * 0.45);
        const float frequency = std::clamp(cutoffHz, 20.0f, maxHz);
        const float resonance = std::clamp(rq, 0.05f, 1.0f);
        if (filter.enabled && filter.cutoffHz == frequency && filter.rq == resonance &&
            filter.mode == mode)
            return;
        const double omega = 2.0 * kPi * frequency / sampleRate_;
        const double cosine = std::cos(omega);
        const double sine = std::sin(omega);
        const double q = 1.0 / resonance;
        const double alpha = sine / (2.0 * q);
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        if (mode == 0) {
            b0 = (1.0 - cosine) * 0.5; b1 = 1.0 - cosine; b2 = b0;
        } else if (mode == 1) {
            b0 = (1.0 + cosine) * 0.5; b1 = -(1.0 + cosine); b2 = b0;
        } else {
            b0 = alpha; b1 = 0.0; b2 = -alpha;
        }
        const double a0 = 1.0 + alpha;
        filter.b0 = static_cast<float>(b0 / a0);
        filter.b1 = static_cast<float>(b1 / a0);
        filter.b2 = static_cast<float>(b2 / a0);
        filter.a1 = static_cast<float>(-2.0 * cosine / a0);
        filter.a2 = static_cast<float>((1.0 - alpha) / a0);
        filter.cutoffHz = frequency;
        filter.rq = resonance;
        filter.mode = mode;
        filter.enabled = true;
    }

    static float processBiquad(BiquadState& filter, float input) {
        float filtered = input;
        if (filter.enabled) {
            filtered = filter.b0 * input + filter.z1;
            filter.z1 = filter.b1 * input - filter.a1 * filtered + filter.z2;
            filter.z2 = filter.b2 * input - filter.a2 * filtered;
            if (!std::isfinite(filtered)) {
                filter.z1 = 0.0f;
                filter.z2 = 0.0f;
                filtered = 0.0f;
            }
        }
        filter.wet += 0.0025f * ((filter.enabled ? 1.0f : 0.0f) - filter.wet);
        return input + (filtered - input) * filter.wet;
    }

    static bool reverbEnabled(const PatternState& pattern) {
        return pattern.reverbIndex >= 1 && pattern.reverbIndex <= 3 &&
               pattern.reverbRoom > 0.0f && pattern.reverbMix > 0.0f;
    }

    void updatePlayerReverb(std::size_t player) {
        if (player >= kPlayerCount)
            return;
        const auto& pattern = activePatterns_[player];
        const bool enabled = reverbEnabled(pattern);
        const auto bus = kReverbBaseBus + static_cast<int>(player) * kReverbBusStride;
        for (int type = 0; type < 3; ++type) {
            auto* graph = reverbGraphs_[player][static_cast<std::size_t>(type)];
            if (!graph)
                continue;
            Graph_SetControl(graph, 0, static_cast<float>(bus));
            Graph_SetControl(graph, 1, static_cast<float>(bus + 2));
            if (type < 2) {
                Graph_SetControl(graph, 2, pattern.reverbRoom);
                Graph_SetControl(graph, 3, pattern.reverbDamp);
            } else {
                Graph_SetControl(graph, 2, 1.0f + pattern.reverbRoom * 99.0f);
                Graph_SetControl(graph, 3, pattern.reverbTime);
                Graph_SetControl(graph, 4, pattern.reverbDamp);
                Graph_SetControl(graph, 5, pattern.reverbSpread);
            }
            Node_SetRun(&graph->mNode, enabled && type == pattern.reverbIndex - 1 ? 1 : 0);
        }
        for (auto& voice : audioVoices_) {
            if (voice.activeSynth && voice.playerIndex == static_cast<int>(player)) {
                Graph_SetControl(voice.activeSynth, 6, static_cast<float>(bus));
                Graph_SetControl(voice.activeSynth, 7, enabled ? 1.0f : 0.0f);
                Graph_SetControl(voice.activeSynth, 8, voice.cutoffHz);
            }
        }
    }

    bool initializeWorld(double hostSampleRate = 0.0) {
        LifecycleTrace trace("initializeWorld including locks", this);
        std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
        if (hostSampleRate > 0.0) {
            if (world_ && sampleRate_ == hostSampleRate)
                return true;
            // Keep teardown, sample-rate change and creation under one lock so
            // a Run worker cannot initialize at the previous host sample rate.
            stopWorldLocked();
            sampleRate_ = hostSampleRate;
            hostProcessingConfigured_ = true;
        } else if (world_) {
            return true;
        }
        auto fail = [this](std::string message) {
            worldInitializationError_ = std::move(message);
            setEditorStatus(worldInitializationError_);
            return false;
        };
        if (!hostProcessingConfigured_)
            return fail("The host has not configured audio processing yet. Enable the DAW audio engine (or start playback), then press Run again.");
        worldInitializationError_.clear();
        setEditorStatus("Starting the embedded SuperCollider engine...");
        std::lock_guard<std::mutex> lifecycleLock(scLibraryLifecycleMutex);
        LifecycleTrace lockedTrace("initializeWorld locks acquired", this);
        const auto ugenPath = bundledUGenPath();
        std::error_code pathError;
        if (ugenPath.empty() || !std::filesystem::is_directory(ugenPath, pathError)) {
            std::fprintf(stderr, "Dexter VST3: bundled SuperCollider UGen directory is missing.\n");
            return fail("SuperCollider startup failed: the bundled SCPlugins folder is missing or inaccessible. Restore the UGen files, restart the host, and press Run.");
        }
        ugenPluginPath_ = ugenModuleSearchPath();

        try {
            LifecycleTrace allocationTrace("echo buffer allocation", this);
            echoBuffer_.assign(kPlayerCount * 2 * kMaxEchoBufferSamples, 0.0f);
        } catch (...) {
            std::fprintf(stderr, "Dexter VST3: could not allocate the player echo buffer.\n");
            return fail("SuperCollider startup failed: could not allocate the player echo buffers. Close unused plugin instances and press Run to retry.");
        }

        WorldOptions options;
        options.mRealTime = false;
        options.mPreferredSampleRate = sampleRate_;
        options.mBufLength = kSCBlockSize;
        options.mMaxNodes = static_cast<uint32>(kWorldNodeCapacity);
        options.mMaxGraphDefs = static_cast<uint32>(kWorldGraphDefCapacity);
        // Size both SuperCollider pools from the actual embedded graph bank,
        // voice slots, and reverb bank; adding a registered synth grows them.
        options.mRealTimeMemorySize = 65536;
        options.mNumInputBusChannels = 0;
        options.mNumOutputBusChannels = 2;
        options.mNumAudioBusChannels = kNumAudioBusChannels;
        options.mLoadGraphDefs = 0;
        options.mRendezvous = false;
        options.mVerbosity = -1;
        options.mUGensPluginPath = ugenPluginPath_.c_str();

        {
            LifecycleTrace worldTrace("World_New", this);
            world_ = World_New(&options);
        }
        if (!world_) {
            std::fprintf(stderr, "Dexter VST3: World_New failed at %.1f Hz.\n", sampleRate_);
            return fail("SuperCollider startup failed: World_New could not create the engine at " +
                        std::to_string(sampleRate_) + " Hz. Check the complete matching UGen bundle and press Run to retry.");
        }
        const auto moduleFiles = discoverUGenModuleFiles();
        std::string moduleStatus = "UGen module files found (" + std::to_string(moduleFiles.size()) + "): ";
        for (std::size_t index = 0; index < moduleFiles.size(); ++index) {
            if (index != 0)
                moduleStatus += ", ";
            moduleStatus += moduleFiles[index];
        }
        moduleStatus += ". Compatible Portable/SCPlugins modules load at startup; restart the DAW after adding one.";
        World_SetSampleRate(world_, sampleRate_);
        {
            LifecycleTrace startTrace("World_Start", this);
            World_Start(world_);
        }

        LifecycleTrace definitionsTrace("synth definitions and reverb bank", this);
        auto* defs = GraphDef_Recv(world_, reinterpret_cast<const char*>(kToneSynthDef),
                                   sizeof(kToneSynthDef), nullptr);
        if (!defs) {
            std::fprintf(stderr, "Dexter VST3: GraphDef_Recv rejected the embedded synth bank.\n");
            discardPartialWorld();
            return fail("SuperCollider startup failed: the base synth definitions could not be loaded. A required UGen may be missing or incompatible. Restore the complete matching plugin/UGen bundle and restart the host.");
        }
        GraphDef_Define(world_, defs);
        auto* coreDefs = GraphDef_Recv(world_, reinterpret_cast<const char*>(kFoxDotSynthDefs),
                                      sizeof(kFoxDotSynthDefs), nullptr);
        if (!coreDefs) {
            std::fprintf(stderr, "Dexter VST3: rejected the additional FoxDot synth bank.\n");
            discardPartialWorld();
            return fail("SuperCollider startup failed: the new FoxDot synth bank could not be loaded. A required UGen may be missing or incompatible. Restore the complete matching plugin/UGen bundle and restart the host.");
        }
        GraphDef_Define(world_, coreDefs);
        auto* sampleDefs = GraphDef_Recv(world_, reinterpret_cast<const char*>(kSampleSynthDef),
                                         sizeof(kSampleSynthDef), nullptr);
        if (!sampleDefs) {
            discardPartialWorld();
            return fail("SuperCollider startup failed: native sample playback definitions could not be loaded.");
        }
        GraphDef_Define(world_, sampleDefs);

        for (std::size_t index = 0; index < kSynthGraphNames.size(); ++index) {
            int32 synthName[4]{};
            std::memcpy(synthName, kSynthGraphNames[index],
                        std::min<std::size_t>(std::strlen(kSynthGraphNames[index]),
                                              sizeof(synthName)));
            synthDefinitions_[index] = World_GetGraphDef(world_, synthName);
        }
        rootGroup_ = World_GetGroup(world_, 0);
        if (std::any_of(synthDefinitions_.begin(), synthDefinitions_.end(),
                        [](const auto* definition) { return definition == nullptr; }) ||
            !rootGroup_) {
            std::string missing;
            for (std::size_t index = 0; index < synthDefinitions_.size(); ++index) {
                if (!synthDefinitions_[index])
                    std::fprintf(stderr, "Dexter VST3: missing embedded synth definition %s.\n",
                                 kSynthGraphNames[index]);
                if (!synthDefinitions_[index]) {
                    if (!missing.empty()) missing += ", ";
                    missing += kSynthGraphNames[index];
                }
            }
            if (!rootGroup_)
                std::fprintf(stderr, "Dexter VST3: root group is unavailable.\n");
            discardPartialWorld();
            return fail("SuperCollider startup failed: " +
                        (missing.empty() ? std::string("the root group is unavailable.") :
                         "missing synth definitions: " + missing + ".") +
                        " Press Run to retry initialization.");
        }
        auto* reverbDefs = GraphDef_Recv(world_, reinterpret_cast<const char*>(kReverbSynthDef),
                                         sizeof(kReverbSynthDef), nullptr);
        if (!reverbDefs) {
            std::fprintf(stderr, "Dexter VST3: GraphDef_Recv rejected the embedded reverb bank.\n");
            discardPartialWorld();
            return fail("SuperCollider startup failed: the reverb definitions could not be loaded. Check the matching ReverbUGens module and restart the host.");
        }
        GraphDef_Define(world_, reverbDefs);
        const std::array<const char*, kReverbGraphCount> reverbNames{{
            "DexRevFree", "DexRevFree2", "DexRevGverb"}};
        int32 reverbNodeId = 100;
        for (std::size_t type = 0; type < reverbNames.size(); ++type) {
            int32 reverbName[4]{};
            std::memcpy(reverbName, reverbNames[type], std::strlen(reverbNames[type]));
            auto* reverbDefinition = World_GetGraphDef(world_, reverbName);
            if (!reverbDefinition) {
                std::fprintf(stderr, "Dexter VST3: missing reverb definition %s.\n",
                             reverbNames[type]);
                discardPartialWorld();
                return fail("SuperCollider startup failed: missing reverb definition " +
                            std::string(reverbNames[type]) + ". Press Run to retry.");
            }
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                auto*& graph = reverbGraphs_[player][type];
                LifecycleTrace reverbTrace(reverbNames[type], this);
                sc_msg_iter noControls(0, "");
                const auto graphError =
                    Graph_New(world_, reverbDefinition, reverbNodeId++, &noControls,
                              &graph, true);
                if (graphError != 0 || !graph) {
                    std::fprintf(stderr,
                                 "Dexter VST3: Graph_New failed for %s (player %zu, error %d).\n",
                                 reverbNames[type], player,
                                 static_cast<int>(graphError));
                    discardPartialWorld();
                    return fail("SuperCollider startup failed: could not create " +
                                std::string(reverbNames[type]) + " for player " +
                                std::to_string(player + 1) + " (SC error " +
                                std::to_string(static_cast<int>(graphError)) +
                                "). Press Run to retry.");
                }
                const auto bus = kReverbBaseBus + static_cast<int>(player) * kReverbBusStride;
                Graph_SetControl(graph, 0, static_cast<float>(bus));
                Graph_SetControl(graph, 1, static_cast<float>(bus + 2));
                if (type < 2) {
                    Graph_SetControl(graph, 2, 0.5f);
                    Graph_SetControl(graph, 3, 0.5f);
                } else {
                    Graph_SetControl(graph, 2, 50.0f);
                    Graph_SetControl(graph, 3, 3.0f);
                    Graph_SetControl(graph, 4, 0.5f);
                    Graph_SetControl(graph, 5, 15.0f);
                }
                Group_AddTail(rootGroup_, &graph->mNode);
                Node_SetRun(&graph->mNode, 0);
            }
        }
        // Synth voice graphs are created lazily by the script worker. IDs
        // follow the small startup reverb bank and are unique within the world.
        nextDynamicGraphNodeId_ = reverbNodeId;
        cursor_ = kSCBlockSize;
        worldInitializationError_.clear();
        setEditorStatus("SuperCollider ready. Press Run to evaluate the script. " + moduleStatus);
        return true;
    }

    void discardPartialWorld() {
        if (world_) {
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        for (auto& voice : audioVoices_)
            voice = {};
        for (auto& player : reverbGraphs_)
            player.fill(nullptr);
        synthDefinitions_.fill(nullptr);
        rootGroup_ = nullptr;
        nextDynamicGraphNodeId_ = 100;
    }

    static std::size_t echoOffset(std::size_t player, std::size_t channel,
                                  std::size_t sample) {
        return (player * 2 + channel) * kMaxEchoBufferSamples + sample;
    }

    void renderSCBlock(std::uint64_t blockStart) {
        for (auto& voice : audioVoices_) {
            if (!voice.activeSynth || (voice.midiNote < 0 && !voice.releasing) ||
                voice.playerIndex < 0 ||
                voice.playerIndex >= static_cast<int>(kPlayerCount))
                continue;
            const auto player = static_cast<std::size_t>(voice.playerIndex);
            const auto& pattern = activePatterns_[player];
            if (voice.synthIndex >= 31) {
                const auto start = patternStartSamples_[player];
                const double beat = blockStart >= start
                    ? static_cast<double>(blockStart - start) * pattern.bpm /
                          (60.0 * sampleRate_) : 0.0;
                Graph_SetControl(voice.activeSynth, 10,
                    evaluateTimeVar(pattern.ampControl, beat, pattern.amplitude));
                Graph_SetControl(voice.activeSynth, 18,
                    evaluateTimeVar(pattern.cutoffControl, beat, pattern.cutoffHz));
            }
            const float level = reverbEnabled(pattern)
                ? voiceEnvelopeAt(voice, blockStart) *
                      (static_cast<float>(voice.velocity) / 127.0f) * voice.amplitude
                : 0.0f;
            Graph_SetControl(voice.activeSynth, 7, level);
            Graph_SetControl(voice.activeSynth, 8, voice.cutoffHz);
        }
        World_Run(world_);
        for (auto& voice : audioVoices_) {
            if (voice.triggerControlPendingReset && voice.activeSynth) {
                Graph_SetControl(voice.activeSynth, 9, 0.0f);
                voice.triggerControlPendingReset = false;
            }
        }
        const float mixSmoothing = std::exp(
            -1.0f / static_cast<float>(sampleRate_ * kMixSmoothingSeconds));
        const float mouseSmoothing = 1.0f - mixSmoothing;
        for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
            const auto absoluteSample = blockStart + static_cast<std::uint64_t>(sampleIndex);
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                const float target = mutedPlayers_[player] ? 0.0f : 1.0f;
                playerMuteGain_[player] += mouseSmoothing * (target - playerMuteGain_[player]);
            }
            std::array<float, kPlayerCount> playerLeft{};
            std::array<float, kPlayerCount> playerRight{};
            std::array<bool, kPlayerCount> playerHasVoice{};
            std::size_t contributingVoices = 0;
            for (std::size_t voiceIndex = 0; voiceIndex < audioVoices_.size(); ++voiceIndex) {
                auto& voice = audioVoices_[voiceIndex];
                const float envelope = voiceEnvelopeAt(voice, absoluteSample);
                if (envelope <= 0.0f || voice.velocity <= 0 || voice.playerIndex < 0 ||
                    voice.playerIndex >= static_cast<int>(kPlayerCount))
                    continue;
                const auto bus = static_cast<std::size_t>(kFirstAudioBus) +
                                 voiceIndex * static_cast<std::size_t>(kAudioBusStride);
                const auto busLength = static_cast<std::size_t>(world_->mBufLength);
                const auto offset = bus * busLength + static_cast<std::size_t>(sampleIndex);
                const float source = world_->mAudioBus[offset];
                const auto controlPlayerIndex = static_cast<std::size_t>(voice.playerIndex);
                const auto& livePattern = activePatterns_[controlPlayerIndex];
                const auto patternStart = patternStartSamples_[controlPlayerIndex];
                const double beat = livePattern.bpm > 0.0 && absoluteSample >= patternStart
                    ? static_cast<double>(absoluteSample - patternStart) * livePattern.bpm /
                          (60.0 * sampleRate_)
                    : 0.0;
                if (livePattern.ampControl.active && voice.synthIndex < 31) {
                    const float patternAmp = evaluateTimeVar(livePattern.ampControl, beat,
                                                             livePattern.amplitude);
                    const float targetAmp = foxDotAmplitude(voice.synthIndex, patternAmp);
                    if (livePattern.ampControl.linear)
                        voice.amplitude = targetAmp;
                    else
                        voice.amplitude += mouseSmoothing * (targetAmp - voice.amplitude);
                }
                if (voice.mousePan) {
                    const float mouseX = std::clamp(
                        world_->mAudioBus[(bus + 1) * busLength + static_cast<std::size_t>(sampleIndex)],
                        0.0f, 1.0f);
                    const float targetPan = mouseX * 2.0f - 1.0f;
                    voice.pan += mouseSmoothing * (targetPan - voice.pan);
                    voice.leftGain = std::sqrt(std::max(0.0f, (1.0f - voice.pan) * 0.5f));
                    voice.rightGain = std::sqrt(std::max(0.0f, (1.0f + voice.pan) * 0.5f));
                } else if (livePattern.panControl.active) {
                    const float targetPan = evaluateTimeVar(livePattern.panControl, beat,
                                                            livePattern.pan);
                    if (livePattern.panControl.linear)
                        voice.pan = targetPan;
                    else
                        voice.pan += mouseSmoothing * (targetPan - voice.pan);
                    voice.leftGain = std::sqrt(std::max(0.0f, (1.0f - voice.pan) * 0.5f));
                    voice.rightGain = std::sqrt(std::max(0.0f, (1.0f + voice.pan) * 0.5f));
                }
                bool dynamicCutoff = voice.mouseCutoff;
                float targetCutoff = voice.cutoffHz;
                if (voice.mouseCutoff) {
                    const float mouseY = std::clamp(
                        world_->mAudioBus[(bus + 2) * busLength + static_cast<std::size_t>(sampleIndex)],
                        0.0f, 1.0f);
                    targetCutoff = 200.0f + mouseY * 7800.0f;
                } else if (livePattern.cutoffControl.active) {
                    targetCutoff = evaluateTimeVar(livePattern.cutoffControl, beat,
                                                   livePattern.cutoffHz);
                    dynamicCutoff = true;
                }
                if (dynamicCutoff) {
                    if (!livePattern.cutoffControl.linear || voice.mouseCutoff)
                        voice.cutoffHz += mouseSmoothing * (targetCutoff - voice.cutoffHz);
                    else
                        voice.cutoffHz = targetCutoff;
                    const double maxCutoff = sampleRate_ * 0.45;
                    const double clampedCutoff = std::min<double>(voice.cutoffHz, maxCutoff);
                    voice.filterCoefficient = clampedCutoff >= maxCutoff
                        ? 1.0f
                        : static_cast<float>(1.0 - std::exp(-2.0 * kPi * clampedCutoff / sampleRate_));
                }
                // New source graphs own cutoff filtering; do not apply a
                // second plugin-only low-pass at the same cutoff.
                if (voice.synthIndex >= 31)
                    voice.filterState = source;
                else
                    voice.filterState += voice.filterCoefficient * (source - voice.filterState);
                const float velocity = static_cast<float>(voice.velocity) / 127.0f;
                auto updateFilterControl = [&](float& current, const TimeVarControl& control, float fallback) {
                    if (!control.active) return;
                    const float target = evaluateTimeVar(control, beat, fallback);
                    if (control.linear) current = target;
                    else current += mouseSmoothing * (target - current);
                };
                updateFilterControl(voice.lpfHz, livePattern.lpfControl, livePattern.lpfHz);
                updateFilterControl(voice.lpr, livePattern.lprControl, livePattern.lpr);
                updateFilterControl(voice.hpfHz, livePattern.hpfControl, livePattern.hpfHz);
                updateFilterControl(voice.hpr, livePattern.hprControl, livePattern.hpr);
                updateFilterControl(voice.bpfHz, livePattern.bpfControl, livePattern.bpfHz);
                updateFilterControl(voice.bpr, livePattern.bprControl, livePattern.bpr);
                updateFilterControl(voice.bpnoise, livePattern.bpnoiseControl,
                                    livePattern.bpnoise);
                updateFilterControl(voice.swell, livePattern.swellControl,
                                    livePattern.swell);
                if (sampleIndex == 0) {
                    const float controlSmoothing = static_cast<float>(
                        1.0 - std::exp(-static_cast<double>(kSCBlockSize) /
                                       (0.01 * sampleRate_)));
                    float targetBpfHz = voice.bpfHz;
                    float targetBpr = voice.bpr;
                    if (voice.bpnoise > 0.0f && voice.bpfHz > 0.0f) {
                        const double sustainSeconds = std::max(
                            0.005, static_cast<double>(voice.endSample -
                                                       voice.startSample) /
                                       sampleRate_);
                        const double noiseRateHz = voice.bpnoise / sustainSeconds;
                        targetBpfHz *= std::exp2(
                            advanceNoise1(voice.bpfNoise, noiseRateHz));
                        targetBpr *= std::exp2(
                            advanceNoise1(voice.bprNoise, noiseRateHz));
                        targetBpr = std::clamp(targetBpr, 0.05f, 1.0f);
                        voice.modulatedBpfHz += controlSmoothing *
                                                (targetBpfHz - voice.modulatedBpfHz);
                        voice.modulatedBpr += controlSmoothing *
                                              (targetBpr - voice.modulatedBpr);
                    } else {
                        voice.modulatedBpfHz = targetBpfHz;
                        voice.modulatedBpr = targetBpr;
                    }
                    const float swellTarget = swellEnvelopeAt(voice, absoluteSample) *
                                              voice.swell * 2000.0f;
                    voice.swellCutoffHz += controlSmoothing *
                                           (swellTarget - voice.swellCutoffHz);
                    configureBiquad(voice.lpfState, voice.lpfHz, voice.lpr, 0);
                    configureBiquad(voice.hpfState, voice.hpfHz, voice.hpr, 1);
                    configureBiquad(voice.bpfState, voice.modulatedBpfHz,
                                    voice.modulatedBpr, 2);
                    configureBiquad(voice.swellState, voice.swellCutoffHz,
                                    voice.hpr, 1);
                }
                float shaped = processBiquad(voice.lpfState, voice.filterState);
                shaped = processBiquad(voice.hpfState, shaped);
                shaped = processBiquad(voice.bpfState, shaped);
                shaped = processBiquad(voice.swellState, shaped);
                const float amplitude = shaped * envelope * velocity * voice.amplitude;
                const auto player = static_cast<std::size_t>(voice.playerIndex);
                playerLeft[player] += amplitude * voice.leftGain;
                playerRight[player] += amplitude * voice.rightGain;
                playerHasVoice[player] = playerMuteGain_[player] > 1e-4f;
                if (playerMuteGain_[player] > 1e-4f)
                    ++contributingVoices;
            }

            float leftMixed = 0.0f;
            float rightMixed = 0.0f;
            const auto writeIndex = static_cast<std::size_t>(absoluteSample % kMaxEchoBufferSamples);
            std::size_t echoTailPlayers = 0;
            std::size_t reverbTailPlayers = 0;
            const auto busLength = static_cast<std::size_t>(world_->mBufLength);
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                const auto& pattern = activePatterns_[player];
                const float dryLeft = playerLeft[player];
                const float dryRight = playerRight[player];
                const float targetReverbMix = reverbEnabled(pattern) ? pattern.reverbMix : 0.0f;
                auto& reverbMix = reverbMixSmoothed_[player];
                reverbMix += (targetReverbMix - reverbMix) * mixSmoothing;
                const auto reverbBus = static_cast<std::size_t>(kReverbBaseBus) +
                                       player * static_cast<std::size_t>(kReverbBusStride);
                const auto reverbOffset = static_cast<std::size_t>(sampleIndex);
                const float wetLeft = world_->mAudioBus[(reverbBus + 2) * busLength + reverbOffset];
                const float wetRight = world_->mAudioBus[(reverbBus + 3) * busLength + reverbOffset];
                if (!playerHasVoice[player] && playerMuteGain_[player] > 1e-4f && reverbMix > 1e-5f &&
                    (std::abs(wetLeft) > 1e-6f || std::abs(wetRight) > 1e-6f))
                    ++reverbTailPlayers;
                const float reverbedLeft = dryLeft * (1.0f - reverbMix) + wetLeft * reverbMix;
                const float reverbedRight = dryRight * (1.0f - reverbMix) + wetRight * reverbMix;
                const auto patternStart = patternStartSamples_[player];
                const double playerBeat = pattern.bpm > 0.0 && absoluteSample >= patternStart
                    ? static_cast<double>(absoluteSample - patternStart) * pattern.bpm /
                          (60.0 * sampleRate_)
                    : 0.0;
                auto& echoMix = echoMixSmoothed_[player];
                const float echoBeats = evaluateTimeVar(pattern.echoControl, playerBeat,
                                                        static_cast<float>(pattern.echoBeats));
                const float requestedDelayTime = evaluateTimeVar(pattern.delayControl, playerBeat,
                                                                  static_cast<float>(pattern.delayTimeSeconds));
                const float targetEchoMix = (echoBeats > 0.0f || requestedDelayTime > 0.0f)
                    ? evaluateTimeVar(pattern.echoMixControl, playerBeat, pattern.echoMix)
                    : 0.0f;
                echoMix += (targetEchoMix - echoMix) * mixSmoothing;
                auto& delaySeconds = echoDelaySmoothed_[player];
                const float targetDelaySeconds = requestedDelayTime > 0.0f
                    ? requestedDelayTime
                    : (pattern.bpm > 0.0 ? echoBeats * 60.0f / static_cast<float>(pattern.bpm) : 0.0f);
                delaySeconds += (targetDelaySeconds - delaySeconds) * mixSmoothing;
                if (sampleIndex % kSCBlockSize == 0) {
                    const float decayBeats = evaluateTimeVar(pattern.echoDecayControl, playerBeat,
                                                            static_cast<float>(pattern.echoDecayBeats));
                    const double decaySeconds = pattern.bpm > 0.0
                        ? static_cast<double>(decayBeats) * 60.0 / pattern.bpm : 0.0;
                    const float requestedFeedback = evaluateTimeVar(pattern.echoFeedbackControl,
                                                                     playerBeat, pattern.echoFeedback);
                    echoFeedbackTarget_[player] = requestedFeedback >= 0.0f
                        ? std::clamp(requestedFeedback, 0.0f, 0.95f)
                        : (decaySeconds > 0.0 && delaySeconds > 0.0f
                            ? std::pow(0.001f, static_cast<float>(delaySeconds / decaySeconds))
                            : 0.0f);
                }
                auto& feedback = echoFeedbackSmoothed_[player];
                feedback += (echoFeedbackTarget_[player] - feedback) * mixSmoothing;
                if (echoMix > 1e-5f && delaySeconds > 0.0f && !echoBuffer_.empty()) {
                    const auto requestedDelay = std::round(delaySeconds * sampleRate_);
                    const auto delaySamples = static_cast<std::size_t>(
                        std::max(1.0, std::min<double>(kMaxEchoBufferSamples - 1,
                                                       requestedDelay)));
                    const auto readIndex = (writeIndex + kMaxEchoBufferSamples - delaySamples) %
                                           kMaxEchoBufferSamples;
                    const auto leftOffset = echoOffset(player, 0, readIndex);
                    const auto rightOffset = echoOffset(player, 1, readIndex);
                    const float delayedLeft = echoBuffer_[leftOffset];
                    const float delayedRight = echoBuffer_[rightOffset];
                    if (!playerHasVoice[player] && playerMuteGain_[player] > 1e-4f &&
                        (std::abs(delayedLeft) > 1e-6f || std::abs(delayedRight) > 1e-6f))
                        ++echoTailPlayers;
                    leftMixed += (reverbedLeft + delayedLeft * echoMix) * playerMuteGain_[player];
                    rightMixed += (reverbedRight + delayedRight * echoMix) * playerMuteGain_[player];
                    echoBuffer_[echoOffset(player, 0, writeIndex)] =
                        reverbedLeft + delayedLeft * feedback;
                    echoBuffer_[echoOffset(player, 1, writeIndex)] =
                        reverbedRight + delayedRight * feedback;
                } else {
                    leftMixed += reverbedLeft * playerMuteGain_[player];
                    rightMixed += reverbedRight * playerMuteGain_[player];
                    echoBuffer_[echoOffset(player, 0, writeIndex)] = 0.0f;
                    echoBuffer_[echoOffset(player, 1, writeIndex)] = 0.0f;
                }
            }

            const auto mixContributors = contributingVoices + echoTailPlayers + reverbTailPlayers;
            const float targetMixGain = mixContributors == 0
                                            ? 1.0f
                                            : 1.0f / static_cast<float>(mixContributors);
            mixGain_ += (targetMixGain - mixGain_) * mixSmoothing;
            outputBlock_[0][static_cast<std::size_t>(sampleIndex)] = leftMixed * mixGain_;
            outputBlock_[1][static_cast<std::size_t>(sampleIndex)] = rightMixed * mixGain_;
        }

        const auto blockEnd = blockStart + static_cast<std::uint64_t>(kSCBlockSize);
        for (auto& voice : audioVoices_) {
            if (voice.reverbTail && voice.releaseEndSample <= blockEnd) {
                deactivateAudioVoice(voice);
            } else if (voice.releasing && !voice.reverbTail &&
                       voice.releaseEndSample <= blockEnd) {
                // SynthDef reverb sends use a control-rate Lag. Keep the graph alive
                // after its voice envelope reaches zero so a later block can set the
                // send to zero and let that Lag finish before stopping the graph.
                beginReverbTail(voice, blockEnd);
            } else if (!voice.releasing && voice.midiNote >= 0 &&
                       voice.endSample <= blockEnd) {
                beginReverbTail(voice, blockEnd);
            }
        }
        ++world_->mBufCounter;
        cursor_ = 0;
    }

    // UGen registration is process-global in this SC revision. Keep modules loaded
    // while any VST instance exists; the VST module terminator unloads them once.
    void stopWorld() {
        LifecycleTrace trace("stopWorld including locks", this);
        std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
        stopWorldLocked();
        hostProcessingConfigured_ = false;
    }

    // Caller owns worldAccessMutex_. Never call the locking wrapper recursively.
    void stopWorldLocked() {
        if (world_) {
            detachSampleBuffers();
            std::lock_guard<std::mutex> lifecycleLock(scLibraryLifecycleMutex);
            LifecycleTrace cleanupTrace("World_Cleanup", this);
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        sampleCache_.clear();
        sampleCacheBytes_ = 0;
        ++sampleWorldGeneration_;
        for (auto& voice : audioVoices_)
            voice = {};
        for (auto& player : reverbGraphs_)
            player.fill(nullptr);
        synthDefinitions_.fill(nullptr);
        rootGroup_ = nullptr;
        nextDynamicGraphNodeId_ = 100;
        cursor_ = kSCBlockSize;
        for (auto& channel : outputBlock_)
            channel.fill(0.0f);
        std::fill(echoBuffer_.begin(), echoBuffer_.end(), 0.0f);
        echoMixSmoothed_.fill(0.0f);
        echoFeedbackTarget_.fill(0.0f);
        echoFeedbackSmoothed_.fill(0.0f);
        reverbMixSmoothed_.fill(0.0f);
        mixGain_ = 0.0f;
        for (auto& pattern : activePatterns_)
            pattern = {};
        for (auto& playerGates : activeMidiGates_)
            playerGates.fill({});
        pendingPlayerMask_ = 0;
        pendingMuteMask_ = 0;
        pendingMuteStates_.fill(false);
        mutedPlayers_.fill(false);
        playerMuteGain_.fill(1.0f);
    }

    std::string scriptNamespace_;
    double sampleRate_ = 48000.0;
    std::mutex worldAccessMutex_;
    bool hostProcessingConfigured_ = false;
    std::string worldInitializationError_;
    World* world_ = nullptr;
    Group* rootGroup_ = nullptr;
    std::array<GraphDef*, kSynthGraphCount> synthDefinitions_{};
    int32 nextDynamicGraphNodeId_ = 100;
    std::array<AudioVoice, kAudioVoiceCount> audioVoices_{};
    std::array<std::array<Graph*, 3>, kPlayerCount> reverbGraphs_{};
    std::string ugenPluginPath_;
    std::array<std::array<float, kSCBlockSize>, 2> outputBlock_{};
    std::vector<float> echoBuffer_;
    std::array<float, kPlayerCount> echoMixSmoothed_{};
    std::array<float, kPlayerCount> echoFeedbackTarget_{};
    std::array<float, kPlayerCount> echoFeedbackSmoothed_{};
    std::array<float, kPlayerCount> reverbMixSmoothed_{};
    std::array<float, kPlayerCount> echoDelaySmoothed_ = [] {
        std::array<float, kPlayerCount> initial{};
        initial.fill(0.25f);
        return initial;
    }();
    float mixGain_ = 0.0f;
    int cursor_ = kSCBlockSize;
    std::mutex patternMutex_;
    std::vector<std::unique_ptr<CachedSample>> sampleCache_;
    std::size_t sampleCacheBytes_ = 0;
    std::uint64_t sampleWorldGeneration_ = 1;
    std::array<PatternState, kPlayerCount> activePatterns_{};
    std::array<PatternState, kPlayerCount> pendingPatterns_{};
    std::uint8_t pendingPlayerMask_ = 0;
    std::array<bool, kPlayerCount> pendingMuteStates_{};
    std::uint8_t pendingMuteMask_ = 0;
    std::array<bool, kPlayerCount> mutedPlayers_{};
    std::array<float, kPlayerCount> playerMuteGain_ = [] {
        std::array<float, kPlayerCount> values{};
        values.fill(1.0f);
        return values;
    }();
    std::array<std::uint64_t, kPlayerCount> patternStartSamples_{};
    std::uint64_t sampleClock_ = 0;
    std::array<std::array<ActiveMidiGate, kMaxPolyphony>, kPlayerCount> activeMidiGates_{};
    std::atomic<bool> stopRequested_{false};
    std::mutex scriptThreadMutex_;
    std::thread scriptThread_;
    std::atomic<bool> scriptRunning_{false};
    std::atomic<std::uint64_t> scriptGeneration_{0};
    std::mutex statusMutex_;
    std::string editorStatus_ = "Ready. Use p1 >> sequence(...) or play().";
    std::mutex editorStateMutex_;
    std::string editorSource_ =
        "tempo(120)\r\nplay([60, 64, 67, 72], step=0.5, duration=0.45)\r\n";
    bool hasEditorSource_ = true;
};

} // namespace Steinberg::Vst

BEGIN_FACTORY_DEF("Dexter U.S.", "", "")

DEF_CLASS2(INLINE_UID(0xC7F03BC1, 0xDEEC47AA, 0x8E28B107, 0xF9B1A553),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           "Dexter U.S FoxDot",
           0,
           "Instrument|Synth",
     "0.4.64",
           kVstVersionString,
           Steinberg::Vst::DexterSCTone::createInstance)

END_FACTORY
