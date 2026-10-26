#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct PythonNoteEvent {
    double beat = 0.0;
    double durationBeats = 1.0;
    int midiNote = 60;
    int velocity = 100;
};

struct PythonPlayerUpdate {
    int playerIndex = 0;
    bool stopped = false;
    double bpm = 120.0;
    double lengthBeats = 1.0;
    int synthIndex = 0;
    double amplitude = 1.0;
    double pan = 0.0;
    double cutoffHz = 20000.0;
    double echoMix = 0.0;
    double echoDelaySeconds = 0.25;
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
