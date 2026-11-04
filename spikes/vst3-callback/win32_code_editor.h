#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"

#include <array>
#include <string>
#include <vector>

struct EditorPlayerState {
    bool active = false;
    bool muted = false;
    int synthIndex = 0;
    int effectIndex = -1;
    std::array<int, 4> effectIndices{{-1, -1, -1, -1}};
    int reverbIndex = 0;
    bool echoEnabled = false;
    bool reverbEnabled = false;
    bool shapeEnabled = false;
    bool driveEnabled = false;
    bool chopEnabled = false;
    bool spinEnabled = false;
};

class ICodeEditorHost {
public:
    virtual ~ICodeEditorHost() = default;
    virtual std::string editorSource() = 0;
    virtual bool hasEditorSource() = 0;
    virtual void setEditorSource(const std::string& source) = 0;
    virtual std::string checkCompatibility(const std::string& source) = 0;
    virtual void submitCode(const std::string& source) = 0;
    virtual void stopPattern() = 0;
    virtual std::string editorStatus() = 0;
    virtual float masterOutputGain() = 0;
    virtual void setMasterOutputGain(float gain) = 0;
    virtual void editorPlayerStates(std::array<EditorPlayerState, 8>& states) = 0;
    virtual std::vector<std::string> editorExternalSynthNames() = 0;
    virtual void stopEditorPlayer(int playerIndex) = 0;
    virtual void setEditorPlayerMuted(int playerIndex, bool muted) = 0;
};

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost* host,
                                          Steinberg::FUnknown* keepAlive);
