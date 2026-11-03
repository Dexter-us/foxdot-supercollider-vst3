#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

struct PythonNoteEvent {
    double beat = 0.0;
    double durationBeats = 1.0;
    int midiNote = 60;
    int velocity = 100;
    double attackSeconds = 0.01;
    double releaseSeconds = 0.04;
    double delayBeats = 0.0;
    int sampleSymbol = 0;
    int sampleIndex = 0;
    double sampleRate = 1.0;
    bool sampleGate = false;
    double sampleStart = 0.0;
    double sampleEnd = 1.0;
    bool sampleLoop = false;
    // Resolved on the script worker; these POD fields are safe to copy in process().
    int sampleBuffer = -1;
    double sampleDurationSeconds = 0.0;
    std::array<int, 8> sampleVariantBuffers{};
    std::array<double, 8> sampleVariantDurations{};
    std::size_t sampleVariantCount = 0;
};

static_assert(std::is_trivially_copyable<PythonNoteEvent>::value,
              "Audio event handoff must not allocate or copy strings on the audio thread");

struct PythonPlayerUpdate {
    std::string ampControlToken = "-";
    std::string panControlToken = "-";
    std::string cutoffControlToken = "-";
    std::string echoControlToken = "-";
    std::string delayTimeControlToken = "-";
    std::string echoDecayControlToken = "-";
    std::string echoMixControlToken = "-";
    std::string echoFeedbackControlToken = "-";
    std::string noteDelayControlToken = "-";
    std::string lpfControlToken = "-";
    std::string lprControlToken = "-";
    std::string hpfControlToken = "-";
    std::string hprControlToken = "-";
    std::string bpfControlToken = "-";
    std::string bprControlToken = "-";
    std::string bpnoiseControlToken = "-";
    std::string swellControlToken = "-";
    std::string shapeControlToken = "-";
    std::string driveControlToken = "-";
    std::string bpmControlToken = "-";
    std::string sampleControlToken = "-";
    std::string sampleRateControlToken = "-";
    std::array<std::string, 7> effectControlTokens{
        "-", "-", "-", "-", "-", "-", "-"
    };
    int playerIndex = 0;
    bool stopped = false;
    double bpm = 120.0;
    double lengthBeats = 1.0;
    int synthIndex = 0;
    int playerEffectUserIndex = -1;
    double synthRate = 0.0;
    double swingSeconds = 0.0;
    // vib, depth, lforate, lfowidth, rq, fmod, blur
    std::array<double, 7> synthControls{0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0};
    double amplitude = 1.0;
    double pan = 0.0;
    double cutoffHz = 20000.0;
    double echoBeats = 0.0;
    double delayTimeSeconds = 0.0;
    double echoDecayBeats = 1.0;
    double echoMix = 0.35;
    double echoFeedback = -1.0;
    int reverbIndex = 1;
    double reverbMix = 0.1;
    double reverbRoom = 0.0;
    double reverbDamp = 0.5;
    double reverbTime = 3.0;
    double reverbSpread = 15.0;
    double lpfHz = 0.0;
    double lpr = 1.0;
    double hpfHz = 0.0;
    double hpr = 1.0;
    double bpfHz = 0.0;
    double bpr = 1.0;
    double bpnoise = 0.0;
    double swell = 0.0;
    double shape = 0.0;
    double drive = 0.0;
    bool mousePan = false;
    bool mouseCutoff = false;
    double attackSeconds = 0.01;
    double releaseSeconds = 0.04;
    std::vector<PythonNoteEvent> events;
};

struct PythonPlayerMute {
    int playerIndex = 0;
    bool muted = false;
};

struct PythonDiskCommand {
    int mode = 0; // stop reader, DiskIn, VDiskIn, DiskOut record, stop recording
    int slot = 0;
    std::string path;
    double rate = 1, amp = 0.5, pan = 0, seconds = 0;
    bool loop = false;
};

struct PythonPattern {
    std::vector<PythonPlayerUpdate> players;
    std::vector<PythonPlayerMute> mutes;
    std::vector<PythonDiskCommand> diskCommands;
};

class PortablePython {
public:
    static PortablePython& instance();

    bool checkCompatibility(const std::filesystem::path& runtimeDirectory,
                            const std::string& source,
                            const std::vector<std::string>& userSynthNames,
                            std::string& report,
                            std::string& error);

    bool evaluate(const std::filesystem::path& runtimeDirectory,
                  const std::string& namespaceName,
                  const std::string& source,
                  const std::vector<std::string>& userSynthNames,
                  PythonPattern& pattern,
                  std::string& error);

private:
    PortablePython() = default;
    bool initialize(const std::filesystem::path& runtimeDirectory, std::string& error);

    bool initialized_ = false;
};
