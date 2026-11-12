#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"
#include "live_player_controls.h"
#include "mic_loop_capture.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

struct EditorPlayerState {
    struct UserSynthControl {
        std::string name;
        float minimum = 0.0f;
        float maximum = 1.0f;
        float normalizedValue = 0.0f;
        bool logarithmic = false;
    };

    static constexpr std::size_t kMaxUserSynthControls = 7;

    bool active = false;
    bool muted = false;
    bool effectMixAvailable = false;
    int synthIndex = 0;
    int effectIndex = -1;
    std::array<int, 4> effectIndices{{-1, -1, -1, -1}};
    std::array<std::array<float, 7>, 4> effectControlValues{};
    std::array<UserSynthControl, kMaxUserSynthControls> userSynthControls{};
    std::size_t userSynthControlCount = 0;
    int reverbIndex = 0;
    bool echoEnabled = false;
    bool reverbEnabled = false;
    bool shapeEnabled = false;
    bool driveEnabled = false;
    bool chopEnabled = false;
    bool spinEnabled = false;
    bool coarseEnabled = false;
    bool formantEnabled = false;
    bool pshiftEnabled = false;
    bool bitcrusherEnabled = false;
    float amplitude = 1.0f;
    float pan = 0.0f;
    float cutoffHz = 20000.0f;
    float effectMix = 0.0f;
    float bits = 0.0f;
    float crush = 1.0f;
    float pshift = 0.0f;
};

class ICodeEditorHost {
public:
    virtual ~ICodeEditorHost() = default;
    virtual std::string editorSource() = 0;
    virtual bool hasEditorSource() = 0;
    virtual void setEditorSource(const std::string& source) = 0;
    virtual std::string superColliderSource() = 0;
    virtual bool hasSuperColliderSource() = 0;
    virtual void setSuperColliderSource(const std::string& source) = 0;
    virtual std::string checkCompatibility(const std::string& source) = 0;
    virtual bool scriptWorkerBusy() = 0;
    virtual void submitCode(const std::string& source) = 0;
    virtual void stopPattern() = 0;
    virtual std::string editorStatus() = 0;
    virtual void submitSuperColliderCode(const std::string& source) = 0;
    virtual void stopSuperCollider() = 0;
    virtual std::string superColliderStatus() = 0;
    virtual float masterOutputGain() = 0;
    virtual void setMasterOutputGain(float gain) = 0;
    virtual void toggleMicLoopRecording() = 0;
    virtual void playMicLoop() = 0;
    virtual void stopMicLoop() = 0;
    virtual dexter::MicLoopState micLoopState() = 0;
    virtual float micLoopInputPeak() = 0;
    virtual void editorPlayerStates(std::array<EditorPlayerState, 8>& states) = 0;
    virtual std::vector<std::string> editorExternalSynthNames() = 0;
    virtual void stopEditorPlayer(int playerIndex) = 0;
    virtual void setEditorPlayerMuted(int playerIndex, bool muted) = 0;
    virtual void setEditorPlayerControl(int playerIndex,
                                        EditorPlayerControl control,
                                        float value) = 0;
    virtual void setEditorPlayerEffectControl(int playerIndex, int effectSlot,
                                              int controlIndex,
                                              float normalizedValue) = 0;
    virtual void setEditorPlayerUserSynthControl(int playerIndex,
                                                 int controlIndex,
                                                 float normalizedValue) = 0;
};

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost* host,
                                          Steinberg::FUnknown* keepAlive);
