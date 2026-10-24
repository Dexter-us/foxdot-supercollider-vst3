#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/ibstream.h"
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
        if (!world_)
            return kResultFalse;

        std::unique_lock<std::mutex> patternLock(patternMutex_, std::try_to_lock);
        if (patternLock.owns_lock()) {
            const auto updateMask = pendingPlayerMask_;
            if (updateMask != 0) {
                const auto newStart = sampleClock_ +
                    static_cast<std::uint64_t>(kSCBlockSize - cursor_);
                for (std::size_t player = 0; player < kPlayerCount; ++player) {
                    if ((updateMask & static_cast<std::uint8_t>(1u << player)) == 0)
                        continue;
                    stopPlayerMidiNotes(data.outputEvents, 0, static_cast<int>(player));
                    stopAudioVoices(static_cast<int>(player));
                    activePatterns_[player] = pendingPatterns_[player];
                    patternStartSamples_[player] = newStart;
                }
                pendingPlayerMask_ = 0;
            }
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
                    if (pattern.players.empty()) {
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
                        next.eventCount = player.events.size();
                        std::copy(player.events.begin(), player.events.end(), next.events.begin());
                        totalNotes += player.events.size();
                        updateMask |= static_cast<std::uint8_t>(1u << player.playerIndex);
                    }
                    {
                        std::lock_guard<std::mutex> lock(patternMutex_);
                        for (std::size_t player = 0; player < kPlayerCount; ++player) {
                            if (updateMask & static_cast<std::uint8_t>(1u << player))
                                pendingPatterns_[player] = updates[player];
                        }
                        pendingPlayerMask_ |= updateMask;
                    }
                    setEditorStatus("Updated " + std::to_string(pattern.players.size()) +
                                    " player(s), " + std::to_string(totalNotes) +
                                    " notes at " + std::to_string(pattern.players.front().bpm) +
                                    " BPM.");
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
    static constexpr std::size_t kAudioVoiceCount = kPlayerCount * kMaxPolyphony + kPlayerCount;
    static constexpr int kFirstAudioBus = 2;
    static constexpr int kNumAudioBusChannels = 128;
    static constexpr double kAttackSeconds = 0.005;
    static constexpr double kReleaseSeconds = 0.012;
    static_assert(kFirstAudioBus + kAudioVoiceCount <= kNumAudioBusChannels,
                  "The SuperCollider world needs one private audio bus per synth voice.");
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
        bool active = false;
    };

    struct TimedMidiEvent {
        std::uint64_t sample = 0;
        int playerIndex = 0;
        int midiNote = 60;
        int velocity = 100;
        bool noteOn = false;
    };

    struct AudioVoice {
        Graph* synth = nullptr;
        int playerIndex = -1;
        int midiNote = -1;
        int velocity = 0;
        std::uint64_t startSample = 0;
        std::uint64_t endSample = 0;
        bool releasing = false;
        std::uint64_t releaseStartSample = 0;
        std::uint64_t releaseEndSample = 0;
        float releaseStartGain = 0.0f;
    };

    struct ScheduledAudioNote {
        int playerIndex = 0;
        int midiNote = -1;
        int velocity = 0;
        std::uint64_t startSample = 0;
        std::uint64_t endSample = 0;
    };

    void setEditorStatus(std::string status) {
        std::lock_guard<std::mutex> lock(statusMutex_);
        editorStatus_ = std::move(status);
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
                                 int midiNote, int velocity, bool noteOn, int channel = 0) {
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
            event.noteOn.noteId = -1;
        } else {
            event.type = Event::kNoteOffEvent;
            event.noteOff.channel = static_cast<int16>(channel);
            event.noteOff.pitch = static_cast<int16>(midiNote);
            event.noteOff.velocity = static_cast<float>(velocity) / 127.0f;
            event.noteOff.noteId = -1;
            event.noteOff.tuning = 0.0f;
        }
        outputEvents->addEvent(event);
    }

    void stopPlayerMidiNotes(IEventList* outputEvents, int32 sampleOffset, int playerIndex) {
        if (playerIndex < 0 || playerIndex >= static_cast<int>(kPlayerCount))
            return;
        auto& notes = activeMidiNotes_[static_cast<std::size_t>(playerIndex)];
        for (std::size_t note = 0; note < notes.size(); ++note) {
            if (!notes[note])
                continue;
            addMidiNoteEvent(outputEvents, sampleOffset, static_cast<int>(note), 0, false,
                             playerIndex);
            notes[note] = false;
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

    float voiceEnvelopeAt(const AudioVoice& voice, std::uint64_t sample) const {
        if (voice.releasing) {
            if (sample < voice.releaseStartSample)
                return voice.releaseStartGain;
            if (sample >= voice.releaseEndSample)
                return 0.0f;
            const auto releaseLength = voice.releaseEndSample - voice.releaseStartSample;
            const float remaining = static_cast<float>(voice.releaseEndSample - sample);
            return voice.releaseStartGain * remaining /
                   static_cast<float>(releaseLength);
        }
        if (voice.midiNote < 0 || sample < voice.startSample ||
            sample >= voice.endSample)
            return 0.0f;

        const auto duration = voice.endSample - voice.startSample;
        const auto attack = std::min(envelopeSamples(kAttackSeconds), duration / 2);
        const auto release = std::min(envelopeSamples(kReleaseSeconds), duration / 2);
        float gain = 1.0f;
        if (attack > 0 && sample < voice.startSample + attack) {
            gain = std::min(gain, static_cast<float>(sample - voice.startSample) /
                                      static_cast<float>(attack));
        }
        if (release > 0 && sample >= voice.endSample - release) {
            gain = std::min(gain, static_cast<float>(voice.endSample - sample) /
                                      static_cast<float>(release));
        }
        return gain;
    }

    void deactivateAudioVoice(AudioVoice& voice) {
        if (voice.synth)
            Node_SetRun(&voice.synth->mNode, 0);
        auto* synth = voice.synth;
        voice = {};
        voice.synth = synth;
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
                deactivateAudioVoice(voice);
                continue;
            }
            voice.releasing = true;
            voice.releaseStartSample = now;
            voice.releaseEndSample = now + envelopeSamples(kReleaseSeconds);
            voice.releaseStartGain = gain;
            voice.midiNote = -1;
        }
    }

    void collectMidiEvents(std::uint64_t blockStart, std::uint64_t blockEnd,
                           std::array<TimedMidiEvent, kMaxMidiEventsPerBlock>& result,
                           std::size_t& resultCount) const {
        resultCount = 0;
        auto append = [&](std::uint64_t sample, int playerIndex, int note,
                          int velocity, bool noteOn) {
            if (sample < blockStart || sample >= blockEnd || resultCount >= result.size())
                return;
            result[resultCount++] = TimedMidiEvent{sample, playerIndex, note, velocity, noteOn};
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
            const auto firstCycle = std::max<std::int64_t>(
                0, static_cast<std::int64_t>(std::floor(startBeat / pattern.lengthBeats)) - 1);
            const auto lastCycle = static_cast<std::int64_t>(
                std::floor(endBeat / pattern.lengthBeats)) + 1;
            for (std::int64_t cycle = firstCycle; cycle <= lastCycle; ++cycle) {
                const double cycleBeat = static_cast<double>(cycle) * pattern.lengthBeats;
                for (std::size_t i = 0; i < pattern.eventCount; ++i) {
                    const auto& note = pattern.events[i];
                    const auto onSample = patternStart + static_cast<std::uint64_t>(
                        std::llround((cycleBeat + note.beat) * samplesPerBeat));
                    const auto offSample = patternStart + static_cast<std::uint64_t>(
                        std::llround((cycleBeat + note.beat + note.durationBeats) * samplesPerBeat));
                    append(onSample, static_cast<int>(player), note.midiNote, note.velocity, true);
                    append(offSample, static_cast<int>(player), note.midiNote, 0, false);
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
            const auto pitch = static_cast<std::size_t>(event.midiNote);
            auto& active = activeMidiNotes_[player][pitch];
            if (event.noteOn) {
                if (!active) {
                    addMidiNoteEvent(data.outputEvents,
                                     static_cast<int32>(event.sample - sampleClock_),
                                     event.midiNote, event.velocity, true, event.playerIndex);
                    active = true;
                }
            } else if (active) {
                addMidiNoteEvent(data.outputEvents,
                                 static_cast<int32>(event.sample - sampleClock_),
                                 event.midiNote, 0, false, event.playerIndex);
                active = false;
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
            const auto cycle = static_cast<std::int64_t>(
                std::floor(beat / pattern.lengthBeats));
            const double loopPosition =
                beat - static_cast<double>(cycle) * pattern.lengthBeats;
            std::size_t playerVoices = 0;
            for (std::size_t i = 0; i < pattern.eventCount; ++i) {
                const auto& event = pattern.events[i];
                if (loopPosition < event.beat ||
                    loopPosition >= event.beat + event.durationBeats)
                    continue;
                if (playerVoices >= kMaxPolyphony || desiredCount >= desired.size())
                    break;
                const double absoluteStartBeat =
                    static_cast<double>(cycle) * pattern.lengthBeats + event.beat;
                const double absoluteEndBeat = absoluteStartBeat + event.durationBeats;
                desired[desiredCount++] = ScheduledAudioNote{
                    static_cast<int>(player), event.midiNote, event.velocity,
                    patternStart + static_cast<std::uint64_t>(
                        std::llround(absoluteStartBeat * samplesPerBeat)),
                    patternStart + static_cast<std::uint64_t>(
                        std::llround(absoluteEndBeat * samplesPerBeat))};
                ++playerVoices;
            }
        }

        for (auto& voice : audioVoices_) {
            if (voice.releasing && voice.releaseEndSample <= blockStart)
                deactivateAudioVoice(voice);
            else if (voice.midiNote >= 0 && voice.endSample <= blockStart)
                deactivateAudioVoice(voice);
        }

        for (std::size_t i = 0; i < desiredCount; ++i) {
            const auto& note = desired[i];
            auto found = std::find_if(audioVoices_.begin(), audioVoices_.end(),
                                      [&](const AudioVoice& voice) {
                                          return voice.playerIndex == note.playerIndex &&
                                                 voice.midiNote == note.midiNote &&
                                                 voice.startSample == note.startSample;
                                      });
            if (found == audioVoices_.end()) {
                found = std::find_if(audioVoices_.begin(), audioVoices_.end(),
                                     [](const AudioVoice& voice) {
                                         return voice.midiNote < 0 && !voice.releasing;
                                     });
                if (found == audioVoices_.end())
                    continue;
                found->playerIndex = note.playerIndex;
                found->midiNote = note.midiNote;
                found->velocity = note.velocity;
                found->startSample = note.startSample;
                found->endSample = note.endSample;
                found->releasing = false;
                if (found->synth) {
                    const float frequency = 440.0f * std::pow(
                        2.0f, static_cast<float>(note.midiNote - 69) / 12.0f);
                    Graph_SetControl(found->synth, 1, frequency);
                    Node_SetRun(&found->synth->mNode, 1);
                }
            }
            found->velocity = note.velocity;
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
        options.mNumAudioBusChannels = kNumAudioBusChannels;
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
        int32 nodeId = 100;
        std::size_t voiceIndex = 0;
        for (auto& voice : audioVoices_) {
            sc_msg_iter noControls(0, "");
            if (Graph_New(world_, definition, nodeId++, &noControls,
                          &voice.synth, true) != 0 || !voice.synth) {
                discardPartialWorld();
                return false;
            }
            const auto outputBus = kFirstAudioBus + static_cast<int>(voiceIndex);
            Graph_SetControl(voice.synth, 0, static_cast<float>(outputBus));
            Group_AddTail(root, &voice.synth->mNode);
            Node_SetRun(&voice.synth->mNode, 0);
            ++voiceIndex;
        }
        cursor_ = kSCBlockSize;
        return true;
    }

    void discardPartialWorld() {
        if (world_) {
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        for (auto& voice : audioVoices_)
            voice = {};
    }

    void renderSCBlock(std::uint64_t blockStart) {
        World_Run(world_);
        for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
            const auto absoluteSample = blockStart + static_cast<std::uint64_t>(sampleIndex);
            float mixed = 0.0f;
            std::size_t contributingVoices = 0;
            for (std::size_t voiceIndex = 0; voiceIndex < audioVoices_.size(); ++voiceIndex) {
                const auto& voice = audioVoices_[voiceIndex];
                const float envelope = voiceEnvelopeAt(voice, absoluteSample);
                if (envelope <= 0.0f || voice.velocity <= 0)
                    continue;
                const auto bus = static_cast<std::size_t>(kFirstAudioBus) + voiceIndex;
                const auto offset = bus * static_cast<std::size_t>(world_->mBufLength) +
                                    static_cast<std::size_t>(sampleIndex);
                const float velocity = static_cast<float>(voice.velocity) / 127.0f;
                mixed += world_->mAudioBus[offset] * envelope * velocity;
                ++contributingVoices;
            }
            outputBlock_[static_cast<std::size_t>(sampleIndex)] =
                contributingVoices == 0
                    ? 0.0f
                    : mixed / static_cast<float>(contributingVoices);
        }

        const auto blockEnd = blockStart + static_cast<std::uint64_t>(kSCBlockSize);
        for (auto& voice : audioVoices_) {
            if ((voice.releasing && voice.releaseEndSample <= blockEnd) ||
                (voice.midiNote >= 0 && voice.endSample <= blockEnd))
                deactivateAudioVoice(voice);
        }
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
        for (auto& voice : audioVoices_)
            voice = {};
        cursor_ = kSCBlockSize;
        outputBlock_.fill(0.0f);
        for (auto& pattern : activePatterns_)
            pattern = {};
        for (auto& playerNotes : activeMidiNotes_)
            playerNotes.fill(false);
        pendingPlayerMask_ = 0;
    }

    std::string scriptNamespace_;
    double sampleRate_ = 48000.0;
    World* world_ = nullptr;
    std::array<AudioVoice, kAudioVoiceCount> audioVoices_{};
    std::string ugenPluginPath_;
    std::array<float, kSCBlockSize> outputBlock_{};
    int cursor_ = kSCBlockSize;
    std::mutex patternMutex_;
    std::array<PatternState, kPlayerCount> activePatterns_{};
    std::array<PatternState, kPlayerCount> pendingPatterns_{};
    std::uint8_t pendingPlayerMask_ = 0;
    std::array<std::uint64_t, kPlayerCount> patternStartSamples_{};
    std::uint64_t sampleClock_ = 0;
    std::array<std::array<bool, 128>, kPlayerCount> activeMidiNotes_{};
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
           "0.3.0",
           kVstVersionString,
           Steinberg::Vst::DexterSCTone::createInstance)

END_FACTORY
