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

struct PythonPattern {
    double bpm = 120.0;
    double lengthBeats = 1.0;
    std::vector<PythonNoteEvent> events;
};

class PortablePython {
public:
    static PortablePython& instance();

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
