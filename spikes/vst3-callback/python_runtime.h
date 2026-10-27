#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct PythonNoteEvent {
    double beat = 0.0;
    double durationBeats = 1.0;
    int midiNote = 60;
    int velocity = 100;
    double attackSeconds = 0.01;
    double releaseSeconds = 0.04;
    double delayBeats = 0.0;
};

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
    int playerIndex = 0;
    bool stopped = false;
    double bpm = 120.0;
    double lengthBeats = 1.0;
    int synthIndex = 0;
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
    bool mousePan = false;
    bool mouseCutoff = false;
    double attackSeconds = 0.01;
    double releaseSeconds = 0.04;
    std::vector<PythonNoteEvent> events;
};

struct PythonPattern {
    std::vector<PythonPlayerUpdate> players;
};

class PortablePython {
public:
    static PortablePython& instance();

    bool checkCompatibility(const std::filesystem::path& runtimeDirectory,
                            const std::string& source,
                            std::string& report,
                            std::string& error);

    bool evaluate(const std::filesystem::path& runtimeDirectory,
                  const std::string& namespaceName,
                  const std::string& source,
                  PythonPattern& pattern,
                  std::string& error);

private:
    PortablePython() = default;
    bool initialize(const std::filesystem::path& runtimeDirectory, std::string& error);

    bool initialized_ = false;
};
