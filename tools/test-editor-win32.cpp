// Exercise the actual Win32 editor implementation, without starting an engine.
// This is not a substitute for a VSTHost integration/audio test.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include "win32_code_editor.h"
#include "Scintilla.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Host final : ICodeEditorHost {
    std::string python = "p1 >> sine(0)\r\n";
    std::string sc = "// independent SC document\r\n1 + 1;\r\n";
    int submissions = 0;
    bool workerBusy = false;
    dexter::MicLoopState micState = dexter::MicLoopState::Empty;
    float micPeak = 0.0f;
    std::string editorSource() override { return python; }
    bool hasEditorSource() override { return true; }
    void setEditorSource(const std::string& value) override { python = value; }
    std::string superColliderSource() override { return sc; }
    bool hasSuperColliderSource() override { return true; }
    void setSuperColliderSource(const std::string& value) override { sc = value; }
    std::string checkCompatibility(const std::string&) override { return ""; }
    bool scriptWorkerBusy() override { return workerBusy; }
    void submitCode(const std::string&) override { ++submissions; }
    void stopPattern() override {}
    std::string editorStatus() override { return "Idle, engine not started"; }
    void submitSuperColliderCode(const std::string&) override { ++submissions; }
    void stopSuperCollider() override {}
    std::string superColliderStatus() override { return "Idle, language not started"; }
    float masterOutputGain() override { return 1.0f; }
    void setMasterOutputGain(float) override {}
    void toggleMicLoopRecording() override {
        micState = micState == dexter::MicLoopState::Recording
            ? dexter::MicLoopState::Ready : dexter::MicLoopState::Recording;
    }
    void playMicLoop() override { micState = dexter::MicLoopState::Playing; }
    void stopMicLoop() override {
        micState = micState == dexter::MicLoopState::Empty
            ? dexter::MicLoopState::Empty : dexter::MicLoopState::Ready;
    }
    dexter::MicLoopState micLoopState() override { return micState; }
    float micLoopInputPeak() override { return micPeak; }
    void editorPlayerStates(std::array<EditorPlayerState, 8>& states) override { states = {}; }
    std::vector<std::string> editorExternalSynthNames() override { return {}; }
    void stopEditorPlayer(int) override {}
    void setEditorPlayerMuted(int, bool) override {}
    void setEditorPlayerControl(int, EditorPlayerControl, float) override {}
    void setEditorPlayerEffectControl(int, int, int, float) override {}
    void setEditorPlayerUserSynthControl(int, int, float) override {}
};

static std::string text(HWND editor) {
    require(editor != nullptr, "Scintilla child is missing");
    const auto length = SendMessageW(editor, SCI_GETTEXTLENGTH, 0, 0);
    std::string result(static_cast<std::size_t>(length) + 1, '\0');
    SendMessageW(editor, SCI_GETTEXT, result.size(),
                 reinterpret_cast<LPARAM>(result.data()));
    result.resize(static_cast<std::size_t>(length));
    return result;
}

static std::wstring windowText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<std::size_t>(length));
    return result;
}

static void clickTab(HWND view, int id) {
    require(GetDlgItem(view, id) != nullptr, "Editor tab button is missing");
    SendMessageW(view, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
                 reinterpret_cast<LPARAM>(GetDlgItem(view, id)));
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

int main() {
    Host host;
    HWND parent = nullptr;
    Steinberg::IPlugView* view = nullptr;
    try {
        parent = CreateWindowExW(0, L"STATIC", L"Dexter editor smoke host",
            WS_OVERLAPPEDWINDOW, 0, 0, 1100, 700, nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        require(parent != nullptr, "Could not create test host window");
        view = createCodeEditorView(&host, nullptr);
        require(view != nullptr, "createCodeEditorView returned null");
        const auto expectedPython = host.python, expectedSC = host.sc;
        for (int reopen = 0; reopen < 6; ++reopen) {
            const auto start = std::chrono::steady_clock::now();
            require(view->attached(parent, Steinberg::kPlatformTypeHWND) == Steinberg::kResultOk,
                    "Editor attach failed");
            require(std::chrono::steady_clock::now() - start < std::chrono::seconds(5),
                    "Editor attach exceeded five seconds");
            HWND window = FindWindowExW(parent, nullptr, L"DexterSCCodeEditorView", nullptr);
            require(window != nullptr, "Main editor window is missing");
            require(text(GetDlgItem(window, 101)) == expectedPython, "Python document changed on reopen");
            require(GetDlgItem(window, 102) != nullptr, "Run button is missing");
            require(GetDlgItem(window, 2610) != nullptr,
                    "Mic-loop record button is missing");
            require(GetDlgItem(window, 2611) != nullptr,
                    "Mic-loop playback button is missing");
            require(GetDlgItem(window, 2612) != nullptr,
                    "Mic-loop stop button is missing");
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(2610, BN_CLICKED),
                         reinterpret_cast<LPARAM>(GetDlgItem(window, 2610)));
            require(host.micState == dexter::MicLoopState::Recording,
                    "Record button did not arm mic-loop capture");
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(2610, BN_CLICKED),
                         reinterpret_cast<LPARAM>(GetDlgItem(window, 2610)));
            require(host.micState == dexter::MicLoopState::Ready,
                    "Record button did not finish mic-loop capture");
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(2611, BN_CLICKED),
                         reinterpret_cast<LPARAM>(GetDlgItem(window, 2611)));
            require(host.micState == dexter::MicLoopState::Playing,
                    "Play button did not start mic-loop playback");
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(2612, BN_CLICKED),
                         reinterpret_cast<LPARAM>(GetDlgItem(window, 2612)));
            require(host.micState == dexter::MicLoopState::Ready,
                    "Stop button did not stop mic-loop playback");
            host.workerBusy = true;
            SendMessageW(window, WM_TIMER, 1, 0);
            require(!IsWindowEnabled(GetDlgItem(window, 102)),
                    "Run stayed enabled while the script worker was busy");
            require(windowText(GetDlgItem(window, 102)) == L"Busy",
                    "Run did not show a busy indicator while processing");
            host.workerBusy = false;
            SendMessageW(window, WM_TIMER, 1, 0);
            require(IsWindowEnabled(GetDlgItem(window, 102)),
                    "Run did not re-enable after the script worker became idle");
            require(windowText(GetDlgItem(window, 102)) == L"Run",
                    "Run button label did not return to idle");
            require(GetDlgItem(window, 3202) == nullptr,
                    "Second Scintilla editor was eagerly created");
            for (int i = 0; i < 25; ++i) {
                clickTab(window, 3201);
                HWND sc = GetDlgItem(window, 3202);
                require(text(sc) == expectedSC, "SC document changed while switching");
                clickTab(window, 3200);
                require(text(GetDlgItem(window, 101)) == expectedPython, "Python document changed while switching");
            }
            clickTab(window, 3201); // Closing on SC must not replace the Python document.
            require(view->removed() == Steinberg::kResultOk, "Editor removal failed");
            require(FindWindowExW(parent, nullptr, L"DexterSCCodeEditorView", nullptr) == nullptr,
                    "Editor window survived removal");
            require(host.python == expectedPython && host.sc == expectedSC,
                    "Persisted documents were mixed on close");
        }
        require(host.submissions == 0, "Opening/switching editors submitted code unexpectedly");
        view->release();
        view = nullptr;
        DestroyWindow(parent);
        std::puts("PASS: actual Win32 editor opens without engine, lazy SC creation, 150 switches, six reopens, independent documents");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        if (view) { view->removed(); view->release(); }
        if (parent) DestroyWindow(parent);
        return 1;
    }
}
