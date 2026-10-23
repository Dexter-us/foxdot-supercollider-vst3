#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "SC_WorldOptions.h"
#include "SC_Prototypes.h"
#include "SC_HiddenWorld.h"
#include "SC_GraphDef.h"
#include "SC_Group.h"
#include "SC_Graph.h"
#include "SC_Lib_Cintf.h"
#include "tone_synthdef.h"
#include "python_runtime.h"
#include "win32_code_editor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
std::mutex scLibraryLifecycleMutex;
std::atomic<unsigned long long> nextScriptNamespace{1};
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
        return static_cast<IAudioProcessor*>(new DexterSCTone());
    }

    DexterSCTone()
        : scriptNamespace_("_dexter_instance_" +
                           std::to_string(nextScriptNamespace.fetch_add(1))) {}

    ~DexterSCTone() override {
        joinScriptThread();
        stopWorld();
    }

    tresult PLUGIN_API initialize(FUnknown* context) override {
        const auto result = SingleComponentEffect::initialize(context);
        if (result != kResultOk)
            return result;
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        addEventOutput(STR16("MIDI Out"), 16);
        return kResultOk;
    }

    IPlugView* PLUGIN_API createView(FIDString name) override {
        if (!name || std::strcmp(name, ViewType::kEditor) != 0)
            return nullptr;
        return createCodeEditorView(
            this, static_cast<IEditController*>(this));
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
        joinScriptThread();
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
        for (int32 channel = 0; channel < output.numChannels; ++channel) {
            if (output.channelBuffers32[channel])
                std::fill_n(output.channelBuffers32[channel], data.numSamples, 0.0f);
        }

        if (!world_)
            return kResultFalse;

        std::unique_lock<std::mutex> patternLock(patternMutex_, std::try_to_lock);
        if (patternLock.owns_lock()) {
            if (pendingPatternReady_) {
                if (activeMidiNote_ >= 0)
                    addMidiNoteEvent(data.outputEvents, 0, activeMidiNote_, 0, false);
                activePattern_ = pendingPattern_;
                pendingPatternReady_ = false;
                patternStartSample_ = sampleClock_;
                playing_ = true;
                activeAudioNote_ = -1;
                audioGain_ = 0.0f;
                activeMidiNote_ = -1;
                if (synth_)
                    Node_SetRun(&synth_->mNode, 0);
                muteRemainingAudioBlock();
            }
            patternLock.unlock();
        }

        if (stopRequested_.exchange(false, std::memory_order_acq_rel)) {
            if (activeMidiNote_ >= 0)
                addMidiNoteEvent(data.outputEvents, 0, activeMidiNote_, 0, false);
            activeMidiNote_ = -1;
            activeAudioNote_ = -1;
            audioGain_ = 0.0f;
            playing_ = false;
            if (synth_)
                Node_SetRun(&synth_->mNode, 0);
            muteRemainingAudioBlock();
        }

        emitPatternMidi(data);
        for (int32 frame = 0; frame < data.numSamples; ++frame) {
            if (cursor_ == kSCBlockSize) {
                updateSynthForBlock(sampleClock_ + static_cast<std::uint64_t>(frame));
                renderSCBlock();
            }
            const float sample = outputBlock_[cursor_++];
            for (int32 channel = 0; channel < output.numChannels; ++channel) {
                if (output.channelBuffers32[channel])
                    output.channelBuffers32[channel][frame] = sample;
            }
        }
        sampleClock_ += static_cast<std::uint64_t>(data.numSamples);
        output.silenceFlags = 0;
        return kResultOk;
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
                        const auto lastLineStart = message.find_last_of("\r\n");
                        if (lastLineStart != std::string::npos)
                            message = message.substr(lastLineStart + 1);
                        setEditorStatus(message.empty() ? "Python evaluation failed."
                                                        : message.substr(0, 180));
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }

                    if (scriptGeneration_.load(std::memory_order_acquire) != generation) {
                        setEditorStatus("Evaluation cancelled.");
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }

                    if (pattern.events.size() > kMaxPatternEvents) {
                        setEditorStatus("The pattern exceeds the 256-note limit.");
                        scriptRunning_.store(false, std::memory_order_release);
                        return;
                    }

                    PatternState next;
                    next.bpm = pattern.bpm;
                    next.lengthBeats = pattern.lengthBeats;
                    next.eventCount = pattern.events.size();
                    std::copy(pattern.events.begin(), pattern.events.end(), next.events.begin());
                    {
                        std::lock_guard<std::mutex> lock(patternMutex_);
                        pendingPattern_ = next;
                        pendingPatternReady_ = true;
                    }
                    setEditorStatus("Pattern running - " +
                                    std::to_string(pattern.events.size()) +
                                    " notes at " + std::to_string(pattern.bpm) + " BPM.");
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

    std::string editorStatus() override {
        std::lock_guard<std::mutex> lock(statusMutex_);
        return editorStatus_;
    }

    OBJ_METHODS(DexterSCTone, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    static constexpr int kSCBlockSize = 64;
    static constexpr std::size_t kMaxPatternEvents = 256;
    static constexpr std::size_t kMaxMidiEventsPerBlock = 1024;

    struct PatternState {
        std::array<PythonNoteEvent, kMaxPatternEvents> events{};
        std::size_t eventCount = 0;
        double bpm = 120.0;
        double lengthBeats = 1.0;
    };

    struct TimedMidiEvent {
        std::uint64_t sample = 0;
        int midiNote = 60;
        int velocity = 100;
        bool noteOn = false;
    };

    void setEditorStatus(std::string status) {
        std::lock_guard<std::mutex> lock(statusMutex_);
        editorStatus_ = std::move(status);
    }

    void muteRemainingAudioBlock() {
        if (cursor_ < kSCBlockSize) {
            std::fill(outputBlock_.begin() + cursor_, outputBlock_.end(), 0.0f);
        }
    }

    void joinScriptThread() {
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
                                 int midiNote, int velocity, bool noteOn) {
        if (!outputEvents)
            return;
        Event event{};
        event.busIndex = 0;
        event.sampleOffset = sampleOffset;
        event.ppqPosition = -1.0;
        event.flags = 0;
        if (noteOn) {
            event.type = Event::kNoteOnEvent;
            event.noteOn.channel = 0;
            event.noteOn.pitch = static_cast<int16>(midiNote);
            event.noteOn.tuning = 0.0f;
            event.noteOn.velocity = static_cast<float>(velocity) / 127.0f;
            event.noteOn.length = 0;
            event.noteOn.noteId = -1;
        } else {
            event.type = Event::kNoteOffEvent;
            event.noteOff.channel = 0;
            event.noteOff.pitch = static_cast<int16>(midiNote);
            event.noteOff.velocity = static_cast<float>(velocity) / 127.0f;
            event.noteOff.noteId = -1;
            event.noteOff.tuning = 0.0f;
        }
        outputEvents->addEvent(event);
    }

    void collectMidiEvents(std::uint64_t blockStart, std::uint64_t blockEnd,
                           std::array<TimedMidiEvent, kMaxMidiEventsPerBlock>& result,
                           std::size_t& resultCount) const {
        resultCount = 0;
        if (!playing_ || activePattern_.eventCount == 0 ||
            activePattern_.lengthBeats <= 0.0 || activePattern_.bpm <= 0.0)
            return;

        const double samplesPerBeat = sampleRate_ * 60.0 / activePattern_.bpm;
        const double startBeat = static_cast<double>(blockStart - patternStartSample_) / samplesPerBeat;
        const double endBeat = static_cast<double>(blockEnd - patternStartSample_) / samplesPerBeat;
        const auto firstCycle = std::max<std::int64_t>(
            0, static_cast<std::int64_t>(std::floor(startBeat / activePattern_.lengthBeats)) - 1);
        const auto lastCycle = static_cast<std::int64_t>(
            std::floor(endBeat / activePattern_.lengthBeats)) + 1;

        auto append = [&](std::uint64_t sample, int note, int velocity, bool noteOn) {
            if (sample < blockStart || sample >= blockEnd ||
                resultCount >= result.size())
                return;
            result[resultCount++] = TimedMidiEvent{sample, note, velocity, noteOn};
        };

        for (std::int64_t cycle = firstCycle; cycle <= lastCycle; ++cycle) {
            const double cycleBeat = static_cast<double>(cycle) * activePattern_.lengthBeats;
            for (std::size_t i = 0; i < activePattern_.eventCount; ++i) {
                const auto& note = activePattern_.events[i];
                const auto onSample = patternStartSample_ +
                    static_cast<std::uint64_t>(std::llround((cycleBeat + note.beat) * samplesPerBeat));
                const auto offSample = patternStartSample_ +
                    static_cast<std::uint64_t>(std::llround(
                        (cycleBeat + note.beat + note.durationBeats) * samplesPerBeat));
                append(onSample, note.midiNote, note.velocity, true);
                append(offSample, note.midiNote, 0, false);
            }
        }

        std::sort(result.begin(), result.begin() + resultCount,
                  [](const TimedMidiEvent& a, const TimedMidiEvent& b) {
                      if (a.sample != b.sample)
                          return a.sample < b.sample;
                      return !a.noteOn && b.noteOn;
                  });
    }

    void emitPatternMidi(ProcessData& data) {
        std::array<TimedMidiEvent, kMaxMidiEventsPerBlock> events{};
        std::size_t count = 0;
        collectMidiEvents(sampleClock_, sampleClock_ + static_cast<std::uint64_t>(data.numSamples),
                          events, count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            addMidiNoteEvent(data.outputEvents,
                             static_cast<int32>(event.sample - sampleClock_),
                             event.midiNote, event.velocity, event.noteOn);
            if (event.noteOn)
                activeMidiNote_ = event.midiNote;
            else if (activeMidiNote_ == event.midiNote)
                activeMidiNote_ = -1;
        }
    }

    void updateSynthForBlock(std::uint64_t blockStart) {
        if (!synth_)
            return;
        int noteToPlay = -1;
        int velocity = 0;
        double latestStartBeat = -1.0;
        if (playing_ && activePattern_.eventCount > 0 &&
            blockStart + kSCBlockSize / 2 >= patternStartSample_) {
            const double samplesPerBeat = sampleRate_ * 60.0 / activePattern_.bpm;
            const double beat = static_cast<double>(
                blockStart + kSCBlockSize / 2 - patternStartSample_) / samplesPerBeat;
            const double loopPosition = std::fmod(beat, activePattern_.lengthBeats);
            for (std::size_t i = 0; i < activePattern_.eventCount; ++i) {
                const auto& event = activePattern_.events[i];
                if (loopPosition >= event.beat &&
                    loopPosition < event.beat + event.durationBeats &&
                    event.beat >= latestStartBeat) {
                    noteToPlay = event.midiNote;
                    velocity = event.velocity;
                    latestStartBeat = event.beat;
                }
            }
        }

        if (noteToPlay >= 0) {
            if (noteToPlay != activeAudioNote_) {
                const float frequency = 440.0f *
                    std::pow(2.0f, static_cast<float>(noteToPlay - 69) / 12.0f);
                Graph_SetControl(synth_, 1, frequency);
                Node_SetRun(&synth_->mNode, 1);
                activeAudioNote_ = noteToPlay;
            }
            audioGain_ = static_cast<float>(velocity) / 127.0f;
        } else if (activeAudioNote_ >= 0) {
            Node_SetRun(&synth_->mNode, 0);
            activeAudioNote_ = -1;
            audioGain_ = 0.0f;
        }
    }

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
        synth_ = synth;
        Group_AddTail(root, &synth_->mNode);
        Node_SetRun(&synth_->mNode, 0);
        cursor_ = kSCBlockSize;
        return true;
    }

    void discardPartialWorld() {
        if (world_) {
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        synth_ = nullptr;
    }

    void renderSCBlock() {
        World_Run(world_);
        for (int i = 0; i < kSCBlockSize; ++i)
            outputBlock_[static_cast<std::size_t>(i)] = world_->mAudioBus[i] * audioGain_;
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
        synth_ = nullptr;
        cursor_ = kSCBlockSize;
        outputBlock_.fill(0.0f);
        audioGain_ = 0.0f;
        playing_ = false;
        activeAudioNote_ = -1;
        activeMidiNote_ = -1;
    }

    std::string scriptNamespace_;
    double sampleRate_ = 48000.0;
    World* world_ = nullptr;
    Graph* synth_ = nullptr;
    std::string ugenPluginPath_;
    std::array<float, kSCBlockSize> outputBlock_{};
    int cursor_ = kSCBlockSize;
    std::mutex patternMutex_;
    PatternState activePattern_{};
    PatternState pendingPattern_{};
    bool pendingPatternReady_ = false;
    bool playing_ = false;
    std::uint64_t patternStartSample_ = 0;
    std::uint64_t sampleClock_ = 0;
    int activeAudioNote_ = -1;
    int activeMidiNote_ = -1;
    float audioGain_ = 0.0f;
    std::atomic<bool> stopRequested_{false};
    std::mutex scriptThreadMutex_;
    std::thread scriptThread_;
    std::atomic<bool> scriptRunning_{false};
    std::atomic<std::uint64_t> scriptGeneration_{0};
    std::mutex statusMutex_;
    std::string editorStatus_ = "Ready. Use tempo() and play().";
};

} // namespace Steinberg::Vst

BEGIN_FACTORY_DEF("Dexter U.S.", "", "")

DEF_CLASS2(INLINE_UID(0xC7F03BC1, 0xDEEC47AA, 0x8E28B107, 0xF9B1A553),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           "Dexter FoxDot Live",
           0,
           "Instrument|Synth",
           "0.2.0",
           kVstVersionString,
           Steinberg::Vst::DexterSCTone::createInstance)

END_FACTORY
