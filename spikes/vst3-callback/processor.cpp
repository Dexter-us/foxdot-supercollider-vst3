#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

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

        std::array<char, 4> magic{'D', 'X', 'S', '1'};
        std::array<char, 4> flags{};
        encodeUint32(syncToDaw_.load(std::memory_order_acquire) ? 1u : 0u, flags);
        std::array<char, 4> length{};
        encodeUint32(static_cast<std::uint32_t>(source.size()), length);

        if (!writeStreamBytes(stream, magic.data(), static_cast<int32>(magic.size())) ||
            !writeStreamBytes(stream, flags.data(), static_cast<int32>(flags.size())) ||
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
        if (magic != std::array<char, 4>{'D', 'X', 'S', '1'})
            return kResultFalse;

        std::array<char, 4> flags{};
        std::array<char, 4> length{};
        if (!readStreamBytes(stream, flags.data(), static_cast<int32>(flags.size())) ||
            !readStreamBytes(stream, length.data(), static_cast<int32>(length.size())))
            return kResultFalse;
        const auto flagValue = decodeUint32(flags);
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
        syncToDaw_.store((flagValue & 1u) != 0, std::memory_order_release);
        stopPattern();
        return kResultOk;
    }

    uint32 PLUGIN_API getProcessContextRequirements() override {
        return IProcessContextRequirements::kNeedProjectTimeMusic |
               IProcessContextRequirements::kNeedTempo |
               IProcessContextRequirements::kNeedTransportState;
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

        const auto* context = data.processContext;
        const bool syncRequested = syncToDaw_.load(std::memory_order_acquire);
        const bool syncAvailable = context &&
            (context->state & ProcessContext::kTempoValid) &&
            (context->state & ProcessContext::kProjectTimeMusicValid) &&
            std::isfinite(context->tempo) && context->tempo >= 20.0 &&
            context->tempo <= 300.0 && std::isfinite(context->projectTimeMusic);
        const bool useDawSync = syncRequested && syncAvailable;
        const bool transportPlaying = useDawSync &&
            (context->state & ProcessContext::kPlaying) != 0;
        const bool dawTransportResumed = useDawSync && transportPlaying &&
            !lastDawTransportPlaying_;
        dawSyncAvailable_.store(syncAvailable, std::memory_order_release);
        dawTransportPlaying_.store(transportPlaying, std::memory_order_release);
        lastDawTransportPlaying_ = useDawSync && transportPlaying;

        bool newPattern = false;
        {
            std::unique_lock<std::mutex> patternLock(patternMutex_, std::try_to_lock);
            if (patternLock.owns_lock()) {
                if (pendingPatternReady_) {
                    if (activeMidiNote_ >= 0)
                        addMidiNoteEvent(data.outputEvents, 0, activeMidiNote_, 0, false);
                    activePattern_ = pendingPattern_;
                    pendingPatternReady_ = false;
                    playing_ = true;
                    newPattern = true;
                    activeAudioNote_ = -1;
                    audioGain_ = 0.0f;
                    activeMidiNote_ = -1;
                    if (synth_)
                        Node_SetRun(&synth_->mNode, 0);
                    muteRemainingAudioBlock();
                }
            }
        }

        const double bpm = useDawSync ? context->tempo : activePattern_.bpm;
        const double safeBpm = std::isfinite(bpm) && bpm >= 20.0 && bpm <= 300.0
                                   ? bpm
                                   : 120.0;
        const double beatsPerSample = safeBpm / (60.0 * sampleRate_);
        const double blockStartBeat = useDawSync
                                          ? context->projectTimeMusic
                                          : static_cast<double>(sampleClock_) * beatsPerSample;

        if (useDawSync != lastClockUsesDaw_) {
            if (playing_) {
                waitingForDawPlay_ = useDawSync && !transportPlaying;
                if (!waitingForDawPlay_)
                    patternStartBeat_ = blockStartBeat;
            }
            lastClockUsesDaw_ = useDawSync;
        }
        if (newPattern) {
            waitingForDawPlay_ = useDawSync && !transportPlaying;
            if (!waitingForDawPlay_)
                patternStartBeat_ = blockStartBeat;
        }
        if (useDawSync && transportPlaying && waitingForDawPlay_ && playing_) {
            patternStartBeat_ = blockStartBeat;
            waitingForDawPlay_ = false;
        }

        if (stopRequested_.exchange(false, std::memory_order_acq_rel)) {
            if (activeMidiNote_ >= 0)
                addMidiNoteEvent(data.outputEvents, 0, activeMidiNote_, 0, false);
            activeMidiNote_ = -1;
            activeAudioNote_ = -1;
            audioGain_ = 0.0f;
            playing_ = false;
            waitingForDawPlay_ = false;
            if (synth_)
                Node_SetRun(&synth_->mNode, 0);
            muteRemainingAudioBlock();
        }

        const bool patternAudible = playing_ &&
            (!useDawSync || transportPlaying) && !waitingForDawPlay_;
        if (useDawSync && !transportPlaying && activeMidiNote_ >= 0) {
            addMidiNoteEvent(data.outputEvents, 0, activeMidiNote_, 0, false);
            activeMidiNote_ = -1;
        }
        if (!patternAudible && synth_) {
            Node_SetRun(&synth_->mNode, 0);
            activeAudioNote_ = -1;
            audioGain_ = 0.0f;
            muteRemainingAudioBlock();
        }

        emitPatternMidi(data, blockStartBeat, safeBpm, patternAudible,
                        dawTransportResumed && !newPattern);
        for (int32 frame = 0; frame < data.numSamples; ++frame) {
            if (cursor_ == kSCBlockSize) {
                const auto frameBeat = blockStartBeat +
                    static_cast<double>(frame) * beatsPerSample;
                updateSynthForBlock(frameBeat, safeBpm, patternAudible);
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

    bool syncToDaw() override {
        return syncToDaw_.load(std::memory_order_acquire);
    }

    void setSyncToDaw(bool enabled) override {
        syncToDaw_.store(enabled, std::memory_order_release);
    }

    std::string editorStatus() override {
        if (syncToDaw_.load(std::memory_order_acquire)) {
            if (!dawSyncAvailable_.load(std::memory_order_acquire))
                return "DAW sync enabled; host timing unavailable, using tempo().";
            if (!dawTransportPlaying_.load(std::memory_order_acquire))
                return "DAW sync enabled; waiting for DAW Play.";
        }
        std::lock_guard<std::mutex> lock(statusMutex_);
        return editorStatus_;
    }

    OBJ_METHODS(DexterSCTone, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    static constexpr int kSCBlockSize = 64;
    static constexpr std::size_t kMaxPatternEvents = 256;
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

    struct PatternState {
        std::array<PythonNoteEvent, kMaxPatternEvents> events{};
        std::size_t eventCount = 0;
        double bpm = 120.0;
        double lengthBeats = 1.0;
    };

    struct TimedMidiEvent {
        int32 sampleOffset = 0;
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

    void collectMidiEvents(double blockStartBeat, double blockEndBeat, double bpm,
                           int32 blockSamples,
                           std::array<TimedMidiEvent, kMaxMidiEventsPerBlock>& result,
                           std::size_t& resultCount) const {
        resultCount = 0;
        if (!playing_ || activePattern_.eventCount == 0 ||
            activePattern_.lengthBeats <= 0.0 || bpm <= 0.0 ||
            !std::isfinite(blockStartBeat) || !std::isfinite(blockEndBeat))
            return;

        const double samplesPerBeat = sampleRate_ * 60.0 / bpm;
        const double startBeat = blockStartBeat - patternStartBeat_;
        const double endBeat = blockEndBeat - patternStartBeat_;
        if (std::abs(startBeat) > 1.0e9 || std::abs(endBeat) > 1.0e9)
            return;
        const auto firstCycle = static_cast<std::int64_t>(
            std::floor(startBeat / activePattern_.lengthBeats)) - 1;
        const auto lastCycle = static_cast<std::int64_t>(
            std::floor(endBeat / activePattern_.lengthBeats)) + 1;

        auto append = [&](double eventBeat, int note, int velocity, bool noteOn) {
            const auto offset = static_cast<std::int64_t>(
                std::llround((eventBeat - blockStartBeat) * samplesPerBeat));
            if (offset < 0 || offset >= blockSamples || resultCount >= result.size())
                return;
            result[resultCount++] = TimedMidiEvent{
                static_cast<int32>(offset), note, velocity, noteOn};
        };

        for (std::int64_t cycle = firstCycle; cycle <= lastCycle; ++cycle) {
            const double cycleBeat = static_cast<double>(cycle) * activePattern_.lengthBeats;
            for (std::size_t i = 0; i < activePattern_.eventCount; ++i) {
                const auto& note = activePattern_.events[i];
                const auto eventStart = patternStartBeat_ + cycleBeat + note.beat;
                append(eventStart, note.midiNote, note.velocity, true);
                append(eventStart + note.durationBeats, note.midiNote, 0, false);
            }
        }

        std::sort(result.begin(), result.begin() + resultCount,
                  [](const TimedMidiEvent& a, const TimedMidiEvent& b) {
                      if (a.sampleOffset != b.sampleOffset)
                          return a.sampleOffset < b.sampleOffset;
                      return !a.noteOn && b.noteOn;
                  });
    }

    void emitPatternMidi(ProcessData& data, double blockStartBeat, double bpm,
                         bool patternAudible, bool resumeMidi) {
        if (!patternAudible)
            return;
        std::array<TimedMidiEvent, kMaxMidiEventsPerBlock> events{};
        std::size_t count = 0;
        const double blockEndBeat = blockStartBeat +
            static_cast<double>(data.numSamples) * bpm / (60.0 * sampleRate_);
        collectMidiEvents(blockStartBeat, blockEndBeat, bpm, data.numSamples,
                          events, count);
        if (resumeMidi && count < events.size()) {
            int note = -1;
            int velocity = 0;
            if (currentNoteAtBeat(blockStartBeat, note, velocity)) {
                const bool alreadyStartsHere = std::any_of(
                    events.begin(), events.begin() + count,
                    [note](const TimedMidiEvent& event) {
                        return event.sampleOffset == 0 && event.noteOn &&
                               event.midiNote == note;
                    });
                if (!alreadyStartsHere)
                    events[count++] = TimedMidiEvent{0, note, velocity, true};
            }
        }
        std::sort(events.begin(), events.begin() + count,
                  [](const TimedMidiEvent& a, const TimedMidiEvent& b) {
                      if (a.sampleOffset != b.sampleOffset)
                          return a.sampleOffset < b.sampleOffset;
                      return !a.noteOn && b.noteOn;
                  });
        for (std::size_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            addMidiNoteEvent(data.outputEvents, event.sampleOffset,
                             event.midiNote, event.velocity, event.noteOn);
            if (event.noteOn)
                activeMidiNote_ = event.midiNote;
            else if (activeMidiNote_ == event.midiNote)
                activeMidiNote_ = -1;
        }
    }

    bool currentNoteAtBeat(double timelineBeat, int& midiNote,
                           int& velocity) const {
        if (!playing_ || activePattern_.eventCount == 0 ||
            activePattern_.lengthBeats <= 0.0 || !std::isfinite(timelineBeat))
            return false;
        const double beat = timelineBeat - patternStartBeat_;
        if (std::abs(beat) > 1.0e9)
            return false;
        double loopPosition = std::fmod(beat, activePattern_.lengthBeats);
        if (loopPosition < 0.0)
            loopPosition += activePattern_.lengthBeats;

        double latestStartBeat = -1.0;
        midiNote = -1;
        velocity = 0;
        for (std::size_t i = 0; i < activePattern_.eventCount; ++i) {
            const auto& event = activePattern_.events[i];
            if (loopPosition >= event.beat &&
                loopPosition < event.beat + event.durationBeats &&
                event.beat >= latestStartBeat) {
                midiNote = event.midiNote;
                velocity = event.velocity;
                latestStartBeat = event.beat;
            }
        }
        return midiNote >= 0;
    }

    void updateSynthForBlock(double blockStartBeat, double bpm, bool patternAudible) {
        if (!synth_)
            return;
        int noteToPlay = -1;
        int velocity = 0;
        if (patternAudible && bpm > 0.0)
            currentNoteAtBeat(blockStartBeat +
                                  static_cast<double>(kSCBlockSize / 2) *
                                      bpm / (60.0 * sampleRate_),
                              noteToPlay, velocity);

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
        waitingForDawPlay_ = false;
        lastClockUsesDaw_ = false;
        lastDawTransportPlaying_ = false;
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
    double patternStartBeat_ = 0.0;
    bool lastClockUsesDaw_ = false;
    bool waitingForDawPlay_ = false;
    bool lastDawTransportPlaying_ = false;
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
    std::mutex editorStateMutex_;
    std::string editorSource_ =
        "tempo(120)\r\nplay([60, 64, 67, 72], step=0.5, duration=0.45)\r\n";
    bool hasEditorSource_ = true;
    std::atomic<bool> syncToDaw_{false};
    std::atomic<bool> dawSyncAvailable_{false};
    std::atomic<bool> dawTransportPlaying_{false};
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
