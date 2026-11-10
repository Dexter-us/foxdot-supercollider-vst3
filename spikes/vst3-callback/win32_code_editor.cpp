#include "win32_code_editor.h"
#include "lifecycle_trace.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Scintilla.h"
#include "ILexer.h"
#include "Lexilla.h"
#include "SciLexer.h"
#include "master_output_gain.h"
#include "plugin_version.h"
#include "editor_catalog.h"
#include "player_line_edit.h"


namespace {
constexpr wchar_t kWindowClass[] = L"DexterSCCodeEditorView";
constexpr int kEditorId = 101;
constexpr int kRunId = 102;
constexpr int kStopId = 103;
constexpr int kStatusId = 104;
constexpr int kOpenId = 106;
constexpr int kSaveId = 107;
constexpr int kSaveAsId = 108;
constexpr int kBrandId = 109;
constexpr int kCopyErrorId = 110;
constexpr int kFileNameId = 111;
constexpr int kMasterGainSliderId = 112;
constexpr int kMasterGainValueId = 113;
constexpr int kMasterGainLabelId = 114;
constexpr int kPanelTabsId = 115;
constexpr int kPanelListId = 116;
constexpr int kPanelMuteId = 117;
constexpr int kPanelStopId = 118;
constexpr int kPanelSamplesId = 119;
constexpr int kPanelVersionId = 120;
constexpr int kPlayerSliderBaseId = 121;
constexpr int kPythonTabButtonId = 3200;
constexpr int kSuperColliderTabButtonId = 3201;
constexpr int kSuperColliderEditorId = 3202;
constexpr int kPlayerControlLabelBaseId = 128;
constexpr int kPlayerControlValueBaseId = 135;
constexpr int kEffectChainItemDataBase = 100;
constexpr int kEffectChainHeaderDataBase = 200;
constexpr int kActiveEffectSummaryHeaderData = 3000;
constexpr int kContextCatalogSynthBaseId = 2400;
constexpr int kContextCatalogEffectBaseId = 2500;
constexpr int kContextPanelInsertId = 2600;
constexpr int kContextPanelAboutId = 2601;
constexpr UINT kSciPositionFromPointClose = 2023;
constexpr int kViewWidth = 1000;
constexpr int kViewHeight = 600;
constexpr int kMargin = 10;
constexpr int kToolbarHeight = 32;
constexpr int kGainRowHeight = 28;
constexpr int kStatusHeight = 24;
constexpr int kPanelWidth = 270;
constexpr std::size_t kMaxScriptBytes = 256 * 1024;
constexpr std::size_t kMaxScriptCharacters = 65536;
struct SampleBrowserEntry {
    std::string category;
    std::string subcategory;
    std::string sortKey;
    std::wstring label;
};

constexpr std::array<const char*, 58> kEmbeddedSynthNames{
    "sine", "triangle", "saw", "square", "bass", "varsaw", "dirt", "jbass",
    "noise", "dub", "growl", "soprano", "crunch", "lazer", "rave", "bell",
    "gong", "marimba", "pluck", "twang", "donk", "squish", "charm", "spark",
    "blip", "scatter", "orient", "zap", "fuzz", "bug", "ripple", "viola",
    "nylon", "arpy", "dab", "sawbass", "dbass", "prophet", "pads", "pasha",
    "ambi", "sinepad", "keys", "klank", "feel", "glass", "soft", "quin",
    "space", "scratch", "creep", "snick", "swell", "razz", "sitar", "star",
    "karp", "sample"};
constexpr std::array<const wchar_t*, 4> kPanelTabNames{
    L"Players", L"Synths", L"Effects", L"Samples"};
constexpr char kPythonKeywords[] =
    "and as assert async await break class continue def del elif else except finally "
    "for from global if import in is lambda nonlocal not or pass raise return try "
    "while with yield False None True";
constexpr char kSuperColliderKeywords[] =
    "var arg class this thisThread currentEnvironment nil true false inf "
    "SinOsc Saw Pulse TriSaw LFTri Blip WhiteNoise PinkNoise BrownNoise "
    "Out In Env EnvGen Line XLine Lag Lag2 LPF HPF BPF RLPF RHPF "
    "Synth SynthDef Group Bus Buffer Server ServerOptions NetAddr "
    "Pbind Pseq Ppar Pmono Pwhite Prand Pwrand Pfunc Pser Pn Pstutter "
    "Routine Task TempoClock SystemClock AppClock OSCdef CmdPeriod";
constexpr char kFoxDotApiKeywords[] =
    "Clock Scale Root Player P PGroup Pvar PWalk PChain PDelta PSquare var linvar DiskIn VDiskIn DiskOut record play stop sine triangle saw pulse bass varsaw dirt jbass noise "
    "dub growl soprano crunch lazer rave bell gong marimba pluck twang donk squish "
    "charm spark blip scatter orient zap fuzz bug ripple "
    "viola nylon arpy dab sawbass dbass prophet pads pasha ambi sinepad keys "
    "klank feel glass soft quin space scratch creep snick swell razz sitar star karp "
    "effect effects fx_vib fx_depth fx_lforate fx_lfowidth fx_rq fx_blur "
    "fx_mix fx_t60 fx_damp fx_size fx_earlydiff fx_moddepth fx_modfreq "
    "fx_delay fx_diff fx_feedback fx_rate fx_spread fx_tone "
    "fx_center fx_shape fx_phase fx_time fx_period fx_length fx_attack "
     "fx_frequency fx_q fx_stages fx_drive fx_bits "
     "JPverb Greyhole Chorus Flanger Phaser Tremolo SCLOrkPhazerFX "
     "SCLOrkDecimatorFX SCLOrkCrossoverDistortionFX LimutPingPong LimutDuck "
    "LimutDisperser LimutFuzz "
    "vib depth lforate lfowidth rq fmod blur sample "
    "p1 p2 p3 p4 p5 p6 p7 p8 amp dur sus oct pan strum cutoff resonance room reverb "
    "echo delay mute unmute rate start end loop stretch shuffle repeat stutter "
    "palindrome mirror accum swap reverse rotate invert transpose arp splice sort unduplicate replace layer trim ltrim normalise choose zip deepzip deeprzip map pivot true_shuffle shufflets";
constexpr char kFoxDotCompletions[] =
    "Clock P PGroup Pvar PWalk PChain PDelta PSquare Player Root Scale DiskIn VDiskIn DiskOut record slot seconds amp cutoff delay dur echo linvar mute oct pan "
    "play p1 p2 p3 p4 p5 p6 p7 p8 pulse reverb resonance room saw sine stop strum "
    "sus triangle unmute var bass varsaw dirt jbass noise dub growl soprano crunch lazer rave "
    "bell gong marimba pluck twang donk squish charm spark blip scatter orient zap fuzz bug ripple rate "
    "viola nylon arpy dab sawbass dbass prophet pads pasha ambi sinepad keys "
    "klank feel glass soft quin space scratch creep snick swell razz sitar star karp "
    "effect effects fx_vib fx_depth fx_lforate fx_lfowidth fx_rq fx_blur "
    "fx_mix fx_t60 fx_damp fx_size fx_earlydiff fx_moddepth fx_modfreq "
    "fx_delay fx_diff fx_feedback fx_rate fx_spread fx_tone "
    "fx_center fx_shape fx_phase fx_time fx_period fx_length fx_attack "
     "fx_frequency fx_q fx_stages fx_drive fx_bits "
     "JPverb Greyhole Chorus Flanger Phaser Tremolo SCLOrkPhazerFX "
     "SCLOrkDecimatorFX SCLOrkCrossoverDistortionFX LimutPingPong LimutDuck "
    "LimutDisperser LimutFuzz "
    "vib depth lforate lfowidth rq fmod blur sample start end loop stretch "
    "shuffle repeat stutter palindrome mirror accum swap reverse rotate invert transpose arp splice sort unduplicate replace layer trim ltrim normalise choose zip deepzip deeprzip map pivot true_shuffle shufflets";

std::mutex gScintillaMutex;
std::size_t gScintillaViewCount = 0;

bool acquireScintillaClasses(HINSTANCE instance) {
    std::lock_guard<std::mutex> lock(gScintillaMutex);
    if (gScintillaViewCount > 0) {
        ++gScintillaViewCount;
        return true;
    }
    if (!Scintilla_RegisterClasses(instance))
        return false;
    gScintillaViewCount = 1;
    return true;
}

void releaseScintillaClasses(HINSTANCE instance) {
    std::lock_guard<std::mutex> lock(gScintillaMutex);
    if (gScintillaViewCount == 0)
        return;
    if (--gScintillaViewCount == 0) {
        // DLL-owned window classes survive DLL unload unless explicitly removed.
        // Never leave a host with a class pointing into an unloaded VST binary.
        UnregisterClassW(kWindowClass, instance);
        Scintilla_ReleaseResources();
    }
}

constexpr wchar_t kInitialCode[] =
    L"tempo(120)\r\n"
    L"play([60, 64, 67, 72], step=0.5, duration=0.45)\r\n";

std::string toUtf8(const std::wstring& text) {
    if (text.empty())
        return {};
    const int required = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0)
        return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring fromUtf8(const std::string& text) {
    if (text.empty())
        return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0);
    if (required <= 0)
        return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()),
                        result.data(), required);
    return result;
}

class CodeEditorView final : public Steinberg::FObject,
                             public Steinberg::IPlugView {
public:
    CodeEditorView(ICodeEditorHost* host, Steinberg::FUnknown* keepAlive)
        : host_(host), keepAlive_(keepAlive) {
        if (keepAlive_)
            keepAlive_->addRef();
    }

    ~CodeEditorView() override {
        LifecycleTrace trace("editor destructor", this);
        removed();
        if (frame_)
            frame_->release();
        if (keepAlive_)
            keepAlive_->release();
    }

    OBJ_METHODS(CodeEditorView, Steinberg::FObject)
    REFCOUNT_METHODS(Steinberg::FObject)

    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                                  void** object) override {
        if (!object)
            return Steinberg::kInvalidArgument;
        if (std::memcmp(iid, Steinberg::IPlugView::iid, sizeof(Steinberg::TUID)) == 0) {
            *object = static_cast<Steinberg::IPlugView*>(this);
            addRef();
            return Steinberg::kResultOk;
        }
        return Steinberg::FObject::queryInterface(iid, object);
    }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override {
        return type && std::strcmp(type, Steinberg::kPlatformTypeHWND) == 0
                   ? Steinberg::kResultOk
                   : Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API attached(void* parent,
                                            Steinberg::FIDString type) override {
        LifecycleTrace trace("editor attached", this);
        if (!parent || !type || std::strcmp(type, Steinberg::kPlatformTypeHWND) != 0)
            return Steinberg::kResultFalse;
        if (window_)
            return Steinberg::kResultOk;

        if (!scintillaClassesAcquired_) {
            if (!acquireScintillaClasses(moduleInstance())) {
                LifecycleTrace::failure("register Scintilla", GetLastError());
                return Steinberg::kResultFalse;
            }
            scintillaClassesAcquired_ = true;
        }
        if (!registerWindowClass()) {
            LifecycleTrace::failure("register editor window class", GetLastError());
            releaseScintillaClasses(moduleInstance());
            scintillaClassesAcquired_ = false;
            return Steinberg::kResultFalse;
        }

        window_ = CreateWindowExW(0, kWindowClass, L"Dexter U.S FoxDot",
                                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                  0, 0, kViewWidth, kViewHeight,
                                  static_cast<HWND>(parent), nullptr,
                                  moduleInstance(), this);
        if (!window_) {
            LifecycleTrace::failure("create editor window", GetLastError());
            releaseScintillaClasses(moduleInstance());
            scintillaClassesAcquired_ = false;
            return Steinberg::kResultFalse;
        }
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API removed() override {
        LifecycleTrace trace("editor removed", this);
        if (window_) {
            KillTimer(window_, 1);
            if (editor_) {
                currentSource_ = editorSource();
                sourceInitialized_ = true;
                persistActiveDocument();
            }
            DestroyWindow(window_);
            window_ = nullptr;
            editor_ = pythonEditor_ = superColliderEditor_ = nullptr;
            pythonTabButton_ = superColliderTabButton_ = nullptr;
            runButton_ = stopButton_ = openButton_ = saveButton_ = saveAsButton_ =
                status_ = brand_ = fileName_ = copyErrorButton_ =
                    masterGainSlider_ = masterGainValue_ = masterGainLabel_ = nullptr;
        }
        if (scintillaClassesAcquired_) {
            releaseScintillaClasses(moduleInstance());
            scintillaClassesAcquired_ = false;
        }
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API onWheel(float) override { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16, Steinberg::int16,
                                             Steinberg::int16) override {
        return Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16, Steinberg::int16,
                                           Steinberg::int16) override {
        return Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override {
        if (!size)
            return Steinberg::kInvalidArgument;
        *size = Steinberg::ViewRect(0, 0, kViewWidth, kViewHeight);
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* size) override {
        if (!size || !window_)
            return Steinberg::kResultFalse;
        MoveWindow(window_, size->left, size->top, size->getWidth(), size->getHeight(), TRUE);
        layoutChildren(size->getWidth(), size->getHeight());
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) override {
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame* frame) override {
        if (frame)
            frame->addRef();
        if (frame_)
            frame_->release();
        frame_ = frame;
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultFalse; }

    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* size) override {
        if (!size)
            return Steinberg::kInvalidArgument;
        size->right = size->left + kViewWidth;
        size->bottom = size->top + kViewHeight;
        return Steinberg::kResultOk;
    }

private:
    static HINSTANCE moduleInstance() {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&moduleInstance), &module);
        return module;
    }

    static std::filesystem::path portableScriptDirectory() {
        const auto module = moduleInstance();
        if (!module)
            return {};

        std::array<wchar_t, 32768> modulePath{};
        const DWORD length = GetModuleFileNameW(
            module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (length == 0 || length >= modulePath.size())
            return {};

        const auto bundleRoot =
            std::filesystem::path(std::wstring(modulePath.data(), length))
                .parent_path()
                .parent_path()
                .parent_path();
        if (bundleRoot.empty())
            return {};
        return bundleRoot / L"Portable";
    }

    static bool registerWindowClass() {
        WNDCLASSEXW existing{};
        existing.cbSize = sizeof(existing);
        if (GetClassInfoExW(moduleInstance(), kWindowClass, &existing)) {
            if (existing.lpfnWndProc == windowProcedure)
                return true;
            // A different version may have been unloaded by this host.
            if (!UnregisterClassW(kWindowClass, moduleInstance()))
                return false;
        }
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.hInstance = moduleInstance();
        windowClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        return true;
    }

    static LRESULT CALLBACK editorSubclassProcedure(
        HWND editor, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR referenceData) {
        (void)subclassId;
        auto* view = reinterpret_cast<CodeEditorView*>(referenceData);
        if (view && message == WM_CONTEXTMENU) {
            if (editor == view->editor_ &&
                view->showPlayerContextMenu(editor, lParam))
                return 0;
            if (editor == view->panelList_ &&
                view->showPanelCatalogMenu(lParam))
                return 0;
        }
        if (view && editor == view->panelList_) {
            if (message == WM_LBUTTONDOWN)
                view->beginCatalogDrag(lParam);
            else if (message == WM_MOUSEMOVE &&
                     view->updateCatalogDrag(wParam, lParam))
                return 0;
            else if (message == WM_LBUTTONUP &&
                     view->finishCatalogDrag(lParam))
                return 0;
            else if (message == WM_CANCELMODE)
                view->resetCatalogDrag();
            else if (message == WM_CAPTURECHANGED &&
                     reinterpret_cast<HWND>(lParam) != view->panelList_)
                view->resetCatalogDrag();
        }
        return DefSubclassProc(editor, message, wParam, lParam);
    }

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
        auto* view = reinterpret_cast<CodeEditorView*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            view = static_cast<CodeEditorView*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(view));
            view->window_ = window;
        }
        if (!view)
            return DefWindowProcW(window, message, wParam, lParam);

        switch (message) {
        case WM_CREATE:
            return view->createChildren() ? 0 : -1;
        case WM_SIZE: {
            RECT client{};
            GetClientRect(window, &client);
            view->layoutChildren(client.right - client.left, client.bottom - client.top);
            InvalidateRect(window, nullptr, TRUE);
            return 0;
        }
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, GetSysColorBrush(COLOR_WINDOW));
            return 1;
        }
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<const NMHDR*>(lParam);
            if (header && header->hwndFrom == view->panelTabs_ &&
                header->code == TCN_SELCHANGE) {
                view->onPanelTabChanged();
                return 0;
            }
            if (header && header->hwndFrom == view->editor_) {
                const auto* notification = reinterpret_cast<const SCNotification*>(lParam);
                if (notification->nmhdr.code == SCN_MODIFIED &&
                    (notification->modificationType &
                     (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0)
                    view->onEditorChanged();
                else if (notification->nmhdr.code == SCN_CHARADDED &&
                         view->activeTab_ == 0)
                    view->onEditorCharacterAdded(notification->ch);
                else if (notification->nmhdr.code == SCN_UPDATEUI &&
                         view->activeTab_ == 0)
                    view->selectPlayerAtEditorCaret();
                return 0;
            }
            break;
        }
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) == view->masterGainSlider_) {
                const int position = static_cast<int>(
                    SendMessageW(view->masterGainSlider_, TBM_GETPOS, 0, 0));
                if (view->host_)
                    view->host_->setMasterOutputGain(
                        dexterMasterGainFromPercent(position));
                view->updateMasterGainText(position);
                return 0;
            }
            for (std::size_t index = 0;
                 index < view->playerControlSliders_.size(); ++index) {
                if (reinterpret_cast<HWND>(lParam) ==
                    view->playerControlSliders_[index]) {
                    view->onPlayerControlSlider(index,
                                                LOWORD(wParam));
                    return 0;
                }
            }
            break;
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kRunId) {
                view->runSource();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kStopId) {
                if (view->host_) {
                    if (view->activeTab_ == 0)
                        view->host_->stopPattern();
                    else
                        view->host_->stopSuperCollider();
                }
                SetWindowTextW(view->status_, L"Stop sent.");
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED &&
                LOWORD(wParam) == kPythonTabButtonId) {
                view->switchEditorTab(0);
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED &&
                LOWORD(wParam) == kSuperColliderTabButtonId) {
                view->switchEditorTab(1);
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kOpenId) {
                view->openSource();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kSaveId) {
                view->saveSource();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kSaveAsId) {
                view->saveSourceAs();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kCopyErrorId) {
                view->copyLastError();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kPanelMuteId) {
                view->toggleSelectedPlayerMute();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kPanelStopId) {
                view->stopSelectedPlayer();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kPanelSamplesId) {
                view->openSamplesFolder();
                return 0;
            }
            if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == kPanelListId) {
                view->onPanelListSelectionChanged();
                return 0;
            }
            break;
        case WM_TIMER:
            view->refreshStatus();
            if (view->selectedPanelTab() == 0) {
                view->refreshPlayersPanel();
            } else if (view->selectedPanelTab() == 1) {
                view->refreshSynthCatalog();
            } else if (view->selectedPanelTab() == 2) {
                view->refreshEffectsPanel();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            if (view->editor_) {
                view->currentSource_ = view->editorSource();
                view->sourceInitialized_ = true;
                view->persistActiveDocument();
            }
            view->window_ = nullptr;
            view->editor_ = view->pythonEditor_ = view->superColliderEditor_ = nullptr;
            view->pythonTabButton_ = view->superColliderTabButton_ = nullptr;
            view->runButton_ = view->stopButton_ =
                view->openButton_ = view->saveButton_ = view->saveAsButton_ =
                    view->status_ = view->brand_ = view->fileName_ =
                        view->copyErrorButton_ = view->masterGainSlider_ =
                        view->masterGainValue_ = view->masterGainLabel_ =
                        view->panelTabs_ = view->panelList_ =
                        view->panelMuteButton_ = view->panelStopButton_ =
                        view->panelSamplesButton_ = view->panelVersion_ = nullptr;
            view->playerControlLabels_.fill(nullptr);
            view->playerControlSliders_.fill(nullptr);
            view->playerControlValues_.fill(nullptr);
            view->activePlayerSlider_ = -1;
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool createChildren() {
        LifecycleTrace trace("editor create children", this);
        INITCOMMONCONTROLSEX controls{};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
        InitCommonControlsEx(&controls);
        runButton_ = CreateWindowExW(0, L"BUTTON", L"Run",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 56, 26, window_,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRunId)),
                                     moduleInstance(), nullptr);
        stopButton_ = CreateWindowExW(0, L"BUTTON", L"Stop",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 56, 26, window_,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStopId)),
                                      moduleInstance(), nullptr);
        pythonTabButton_ = CreateWindowExW(
            0, L"BUTTON", L"FoxDot / Python",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
            0, 0, 116, 22, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPythonTabButtonId)),
            moduleInstance(), nullptr);
        superColliderTabButton_ = CreateWindowExW(
            0, L"BUTTON", L"SuperCollider .scd",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
            0, 0, 150, 22, window_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(kSuperColliderTabButtonId)),
            moduleInstance(), nullptr);
        openButton_ = CreateWindowExW(0, L"BUTTON", L"Open",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 56, 26, window_,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpenId)),
                                      moduleInstance(), nullptr);
        saveButton_ = CreateWindowExW(0, L"BUTTON", L"Save",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 62, 26, window_,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveId)),
                                      moduleInstance(), nullptr);
        saveAsButton_ = CreateWindowExW(0, L"BUTTON", L"Save As",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                        0, 0, 76, 26, window_,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveAsId)),
                                        moduleInstance(), nullptr);
        copyErrorButton_ = CreateWindowExW(0, L"BUTTON", L"Copy Error",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                            0, 0, 82, 26, window_,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCopyErrorId)),
                                            moduleInstance(), nullptr);
        status_ = CreateWindowExW(0, L"EDIT", L"Ready. Open a script or edit code.",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                      ES_READONLY | ES_AUTOHSCROLL | ES_NOHIDESEL,
                                  0, 0, 300, 24, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)),
                                  moduleInstance(), nullptr);
        const auto brandText =
            fromUtf8("Dexter U.S FoxDot  |  v" DEXTER_PLUGIN_VERSION);
        brand_ = CreateWindowExW(0, L"STATIC", brandText.c_str(),
                                 WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                                 0, 0, 210, kStatusHeight, window_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBrandId)),
                                 moduleInstance(), nullptr);
        fileName_ = CreateWindowExW(0, L"STATIC", L"Untitled script",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS,
                                    0, 0, 300, kStatusHeight, window_,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFileNameId)),
                                    moduleInstance(), nullptr);
        masterGainLabel_ = CreateWindowExW(
            0, L"STATIC", L"Master",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            0, 0, 64, 22, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMasterGainLabelId)),
            moduleInstance(), nullptr);
        masterGainSlider_ = CreateWindowExW(
            0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
            0, 0, 210, 24, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMasterGainSliderId)),
            moduleInstance(), nullptr);
        masterGainValue_ = CreateWindowExW(
            0, L"STATIC", L"100%",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            0, 0, 64, 22, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMasterGainValueId)),
            moduleInstance(), nullptr);
        if (masterGainSlider_) {
            SendMessageW(masterGainSlider_, TBM_SETRANGE, TRUE,
                         MAKELONG(0, kDexterMasterGainMaximumPercent));
            SendMessageW(masterGainSlider_, TBM_SETTICFREQ, 50, 0);
            const int position = host_
                ? static_cast<int>(std::lround(host_->masterOutputGain() * 100.0f))
                : 100;
            SendMessageW(masterGainSlider_, TBM_SETPOS, TRUE, position);
            updateMasterGainText(position);
        }
        panelTabs_ = CreateWindowExW(
            0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
            0, 0, 260, 28, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelTabsId)),
            moduleInstance(), nullptr);
        if (panelTabs_) {
            for (std::size_t index = 0; index < kPanelTabNames.size(); ++index) {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                item.pszText = const_cast<wchar_t*>(kPanelTabNames[index]);
                TabCtrl_InsertItem(panelTabs_, static_cast<int>(index), &item);
            }
            TabCtrl_SetCurSel(panelTabs_, 0);
        }
        panelList_ = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            0, 0, 260, 360, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelListId)),
            moduleInstance(), nullptr);
        if (panelList_)
            SetWindowSubclass(panelList_, editorSubclassProcedure, 2,
                              reinterpret_cast<DWORD_PTR>(this));
        panelMuteButton_ = CreateWindowExW(
            0, L"BUTTON", L"Mute player",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 88, 24, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelMuteId)),
            moduleInstance(), nullptr);
        panelStopButton_ = CreateWindowExW(
            0, L"BUTTON", L"Stop player",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 88, 24, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelStopId)),
            moduleInstance(), nullptr);
        panelSamplesButton_ = CreateWindowExW(
            0, L"BUTTON", L"Open Samples Folder",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 150, 24, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelSamplesId)),
            moduleInstance(), nullptr);
        const auto panelVersionText =
            fromUtf8(std::string("v") + DEXTER_PLUGIN_VERSION);
        panelVersion_ = CreateWindowExW(
            0, L"STATIC", panelVersionText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
            0, 0, 80, 24, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPanelVersionId)),
            moduleInstance(), nullptr);
        static constexpr std::array<const wchar_t*, 7> controlLabels{
            L"Level", L"Pan", L"Cutoff", L"FX mix", L"Bits", L"Crush",
            L"PShift"};
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            playerControlLabels_[index] = CreateWindowExW(
                0, L"STATIC", controlLabels[index],
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                0, 0, 64, 24, window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(
                    kPlayerControlLabelBaseId + index)),
                moduleInstance(), nullptr);
            playerControlSliders_[index] = CreateWindowExW(
                0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                0, 0, 126, 24, window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(
                    kPlayerSliderBaseId + index)),
                moduleInstance(), nullptr);
            playerControlValues_[index] = CreateWindowExW(
                0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                0, 0, 68, 24, window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(
                    kPlayerControlValueBaseId + index)),
                moduleInstance(), nullptr);
            playerControlRanges_[index] = CreateWindowExW(
                0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                0, 0, 96, 16, window_, nullptr, moduleInstance(), nullptr);
        }
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            if (playerControlSliders_[index]) {
                const int minimum = index == 5 ? 1 : 0;
                const int maximum = index == 6 ? 480
                    : index == 2 ? 1000
                    : index == 1 ? 200
                    : index == 4 ? 24
                    : index == 5 ? 128 : 100;
                SendMessageW(playerControlSliders_[index], TBM_SETRANGE,
                             TRUE, MAKELONG(minimum, maximum));
            }
        }
        static constexpr std::array<int, 7> initialPlayerControlPositions{
            30, 100, 500, 50, 0, 1, 240};
        for (std::size_t index = 0;
             index < playerControlSliders_.size(); ++index) {
            SendMessageW(playerControlSliders_[index], TBM_SETPOS, TRUE,
                         initialPlayerControlPositions[index]);
            const auto value = fromUtf8(
                sliderValueText(index, initialPlayerControlPositions[index]));
            SetWindowTextW(playerControlValues_[index], value.c_str());
        }
        if (!pythonSourceInitialized_) {
            pythonSource_ = host_ && host_->hasEditorSource()
                                 ? fromUtf8(host_->editorSource())
                                 : kInitialCode;
            pythonSourceInitialized_ = true;
        }
        if (!superColliderSourceInitialized_) {
            superColliderSource_ = host_ && host_->hasSuperColliderSource()
                ? fromUtf8(host_->superColliderSource())
                : fromUtf8(
                    "// SuperCollider 3.15 code runs in bundled headless sclang.\r\n"
                    "// It connects to this VST's embedded engine, not a second server.\r\n"
                    "// Run only source you trust; this is unrestricted SuperCollider code.\r\n"
                    "s = Server.default;\r\n"
                    "(\r\n"
                    "Routine({\r\n"
                    "    SynthDef(\\dexterSine, { |out=0, freq=440, amp=0.12, gate=1|\r\n"
                    "        var env = EnvGen.kr(Env.asr(0.01, 1, 0.15), gate, doneAction: 2);\r\n"
                    "        Out.ar(out, (SinOsc.ar(freq) * amp * env) ! 2);\r\n"
                    "    }).add;\r\n"
                    "    s.sync;\r\n"
                    "    x = Synth(\\dexterSine);\r\n"
                    "}).play(SystemClock);\r\n"
                    ")\r\n");
            superColliderSourceInitialized_ = true;
        }
        updatingEditor_ = true;
        pythonEditor_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Scintilla", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL,
                                  0, 0, 600, 380, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorId)),
                                  moduleInstance(), nullptr);
        if (pythonEditor_) {
            editor_ = pythonEditor_;
            configureEditor();
            setEditorWindowText(pythonEditor_, pythonSource_);
            SetWindowSubclass(editor_, editorSubclassProcedure, 1,
                              reinterpret_cast<DWORD_PTR>(this));
        } else {
            LifecycleTrace::failure("create Python Scintilla child", GetLastError());
            updatingEditor_ = false;
            return false;
        }
        // The second Scintilla document is created only when its tab is opened.
        // No SuperCollider UI or language process is needed to open Python.
        editor_ = pythonEditor_;
        activeTab_ = 0;
        currentSource_ = pythonSource_;
        currentFilePath_ = pythonFilePath_;
        documentDirty_ = pythonDirty_;
        sourceInitialized_ = pythonSourceInitialized_;
        SendMessageW(pythonTabButton_, BM_SETCHECK, BST_CHECKED, 0);
        updatingEditor_ = false;
        SetTimer(window_, 1, 250, nullptr);
        layoutChildren(kViewWidth, kViewHeight);
        renderPanelTab();
        updateSaveButton();
        return true;
    }

    void configureEditor() {
        SendMessageW(editor_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        SendMessageW(editor_, SCI_SETMODEVENTMASK,
                     SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT, 0);
        SendMessageW(editor_, SCI_SETEOLMODE, SC_EOL_CRLF, 0);
        SendMessageW(editor_, SCI_SETTABWIDTH, 4, 0);
        SendMessageW(editor_, SCI_SETUSETABS, FALSE, 0);
        SendMessageW(editor_, SCI_SETINDENT, 4, 0);
        SendMessageW(editor_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
        SendMessageW(editor_, SCI_SETMARGINWIDTHN, 0, 44);
        SendMessageW(editor_, SCI_STYLECLEARALL, 0, 0);
        SendMessageW(editor_, SCI_STYLESETFONT, STYLE_DEFAULT,
                     reinterpret_cast<LPARAM>("Consolas"));
        SendMessageW(editor_, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
        SendMessageW(editor_, SCI_STYLESETFORE, STYLE_DEFAULT, RGB(35, 39, 43));
        SendMessageW(editor_, SCI_STYLESETBACK, STYLE_DEFAULT, RGB(250, 250, 250));
        SendMessageW(editor_, SCI_STYLESETBACK, STYLE_LINENUMBER, RGB(239, 242, 245));
        SendMessageW(editor_, SCI_STYLESETFORE, STYLE_LINENUMBER, RGB(105, 112, 120));
        SendMessageW(editor_, SCI_SETCARETLINEVISIBLE, TRUE, 0);
        SendMessageW(editor_, SCI_SETCARETLINEBACK, RGB(242, 246, 250), 0);

        ILexer5* pythonLexer = CreateLexer("python");
        if (pythonLexer) {
            SendMessageW(editor_, SCI_SETILEXER, 0,
                         reinterpret_cast<LPARAM>(pythonLexer));
            SendMessageW(editor_, SCI_SETKEYWORDS, 0,
                         reinterpret_cast<LPARAM>(kPythonKeywords));
            SendMessageW(editor_, SCI_SETKEYWORDS, 1,
                         reinterpret_cast<LPARAM>(kFoxDotApiKeywords));
        }

        const auto setStyle = [this](int style, COLORREF foreground, bool bold = false) {
            SendMessageW(editor_, SCI_STYLESETFORE, style, foreground);
            if (bold)
                SendMessageW(editor_, SCI_STYLESETBOLD, style, TRUE);
        };
        setStyle(SCE_P_COMMENTLINE, RGB(0, 126, 60));
        setStyle(SCE_P_COMMENTBLOCK, RGB(0, 126, 60));
        setStyle(SCE_P_NUMBER, RGB(157, 72, 24));
        setStyle(SCE_P_STRING, RGB(163, 44, 44));
        setStyle(SCE_P_CHARACTER, RGB(163, 44, 44));
        setStyle(SCE_P_TRIPLE, RGB(163, 44, 44));
        setStyle(SCE_P_TRIPLEDOUBLE, RGB(163, 44, 44));
        setStyle(SCE_P_FSTRING, RGB(163, 44, 44));
        setStyle(SCE_P_FCHARACTER, RGB(163, 44, 44));
        setStyle(SCE_P_FTRIPLE, RGB(163, 44, 44));
        setStyle(SCE_P_FTRIPLEDOUBLE, RGB(163, 44, 44));
        setStyle(SCE_P_WORD, RGB(38, 75, 145), true);
        setStyle(SCE_P_WORD2, RGB(111, 61, 151), true);
        setStyle(SCE_P_CLASSNAME, RGB(35, 98, 140), true);
        setStyle(SCE_P_DEFNAME, RGB(35, 98, 140), true);
        setStyle(SCE_P_OPERATOR, RGB(75, 79, 84));
        setStyle(SCE_P_DECORATOR, RGB(125, 78, 33));
        setStyle(SCE_P_ATTRIBUTE, RGB(44, 99, 103));
        SendMessageW(editor_, SCI_AUTOCSETIGNORECASE, TRUE, 0);
        SendMessageW(editor_, SCI_AUTOCSETORDER, SC_ORDER_PERFORMSORT, 0);
    }

    void configureSuperColliderEditor() {
        SendMessageW(editor_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        SendMessageW(editor_, SCI_SETMODEVENTMASK,
                     SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT, 0);
        SendMessageW(editor_, SCI_SETEOLMODE, SC_EOL_CRLF, 0);
        SendMessageW(editor_, SCI_SETTABWIDTH, 4, 0);
        SendMessageW(editor_, SCI_SETUSETABS, FALSE, 0);
        SendMessageW(editor_, SCI_SETINDENT, 4, 0);
        SendMessageW(editor_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
        SendMessageW(editor_, SCI_SETMARGINWIDTHN, 0, 44);
        SendMessageW(editor_, SCI_STYLECLEARALL, 0, 0);
        SendMessageW(editor_, SCI_STYLESETFONT, STYLE_DEFAULT,
                     reinterpret_cast<LPARAM>("Consolas"));
        SendMessageW(editor_, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
        SendMessageW(editor_, SCI_STYLESETFORE, STYLE_DEFAULT, RGB(35, 39, 43));
        SendMessageW(editor_, SCI_STYLESETBACK, STYLE_DEFAULT, RGB(250, 250, 250));
        SendMessageW(editor_, SCI_STYLESETBACK, STYLE_LINENUMBER, RGB(239, 242, 245));
        SendMessageW(editor_, SCI_STYLESETFORE, STYLE_LINENUMBER, RGB(105, 112, 120));
        ILexer5* lexer = CreateLexer("cpp");
        if (lexer) {
            SendMessageW(editor_, SCI_SETILEXER, 0,
                         reinterpret_cast<LPARAM>(lexer));
            SendMessageW(editor_, SCI_SETKEYWORDS, 0,
                         reinterpret_cast<LPARAM>(kSuperColliderKeywords));
        }
        const auto setStyle = [this](int style, COLORREF foreground,
                                     bool bold = false) {
            SendMessageW(editor_, SCI_STYLESETFORE, style, foreground);
            if (bold)
                SendMessageW(editor_, SCI_STYLESETBOLD, style, TRUE);
        };
        setStyle(SCE_C_COMMENT, RGB(0, 126, 60));
        setStyle(SCE_C_COMMENTLINE, RGB(0, 126, 60));
        setStyle(SCE_C_COMMENTDOC, RGB(0, 126, 60));
        setStyle(SCE_C_NUMBER, RGB(157, 72, 24));
        setStyle(SCE_C_STRING, RGB(163, 44, 44));
        setStyle(SCE_C_CHARACTER, RGB(163, 44, 44));
        setStyle(SCE_C_WORD, RGB(38, 75, 145), true);
        setStyle(SCE_C_WORD2, RGB(111, 61, 151), true);
        setStyle(SCE_C_OPERATOR, RGB(75, 79, 84));
        setStyle(SCE_C_PREPROCESSOR, RGB(125, 78, 33));
        SendMessageW(editor_, SCI_AUTOCSETIGNORECASE, TRUE, 0);
        SendMessageW(editor_, SCI_AUTOCSETORDER, SC_ORDER_PERFORMSORT, 0);
    }

    void onEditorChanged() {
        if (updatingEditor_)
            return;
        documentDirty_ = true;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        updateSaveButton();
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        if (activeTab_ == 0) {
            pythonSource_ = currentSource_;
            pythonSourceInitialized_ = true;
            pythonDirty_ = documentDirty_;
            if (host_)
                host_->setEditorSource(toUtf8(currentSource_));
        } else {
            superColliderSource_ = currentSource_;
            superColliderSourceInitialized_ = true;
            superColliderDirty_ = documentDirty_;
            if (host_)
                host_->setSuperColliderSource(toUtf8(currentSource_));
        }
    }

    void persistActiveDocument() {
        if (!editor_)
            return;
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        if (activeTab_ == 0) {
            pythonSource_ = currentSource_;
            pythonSourceInitialized_ = true;
            pythonFilePath_ = currentFilePath_;
            pythonDirty_ = documentDirty_;
            if (host_)
                host_->setEditorSource(toUtf8(currentSource_));
        } else {
            superColliderSource_ = currentSource_;
            superColliderSourceInitialized_ = true;
            superColliderFilePath_ = currentFilePath_;
            superColliderDirty_ = documentDirty_;
            if (host_)
                host_->setSuperColliderSource(toUtf8(currentSource_));
        }
    }

    static void setEditorWindowText(HWND target, const std::wstring& source) {
        if (!target)
            return;
        const auto utf8 = toUtf8(source);
        SendMessageW(target, SCI_SETTEXT, 0,
                     reinterpret_cast<LPARAM>(utf8.c_str()));
    }

    void switchEditorTab(int tab) {
        if (tab < 0 || tab > 1 || tab == activeTab_ || !editor_ ||
            (tab == 0 && !pythonEditor_))
            return;
        if (tab == 1 && !superColliderEditor_) {
            LifecycleTrace trace("editor create SuperCollider child", this);
            superColliderEditor_ = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"Scintilla", L"",
                WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL,
                0, 0, 600, 380, window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSuperColliderEditorId)),
                moduleInstance(), nullptr);
            if (!superColliderEditor_) {
                const auto error = GetLastError();
                LifecycleTrace::failure("create SuperCollider Scintilla child", error);
                const auto message = L"Could not open the SuperCollider editor (Windows error " +
                    std::to_wstring(error) + L"). The Python editor is still available.";
                SetWindowTextW(status_, message.c_str());
                return;
            }
            const auto previous = editor_;
            editor_ = superColliderEditor_;
            updatingEditor_ = true;
            configureSuperColliderEditor();
            setEditorWindowText(superColliderEditor_, superColliderSource_);
            SetWindowSubclass(editor_, editorSubclassProcedure, 1,
                              reinterpret_cast<DWORD_PTR>(this));
            updatingEditor_ = false;
            editor_ = previous;
        }
        persistActiveDocument();
        activeTab_ = tab;
        if (activeTab_ == 0) {
            editor_ = pythonEditor_;
            currentSource_ = pythonSource_;
            currentFilePath_ = pythonFilePath_;
            documentDirty_ = pythonDirty_;
            sourceInitialized_ = pythonSourceInitialized_;
            ShowWindow(pythonEditor_, SW_SHOW);
            ShowWindow(superColliderEditor_, SW_HIDE);
            SendMessageW(pythonTabButton_, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(superColliderTabButton_, BM_SETCHECK, BST_UNCHECKED, 0);
        } else {
            editor_ = superColliderEditor_;
            currentSource_ = superColliderSource_;
            currentFilePath_ = superColliderFilePath_;
            documentDirty_ = superColliderDirty_;
            sourceInitialized_ = superColliderSourceInitialized_;
            ShowWindow(pythonEditor_, SW_HIDE);
            ShowWindow(superColliderEditor_, SW_SHOW);
            SendMessageW(pythonTabButton_, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(superColliderTabButton_, BM_SETCHECK, BST_CHECKED, 0);
        }
        updatingEditor_ = true;
        setEditorWindowText(editor_, currentSource_);
        updatingEditor_ = false;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        updateSaveButton();
        const auto status = activeTab_ == 0
            ? (host_ ? host_->editorStatus() : "FoxDot/Python editor ready.")
            : (host_ ? host_->superColliderStatus()
                     : "Bundled headless sclang is available in the Windows VST3.");
        SetWindowTextW(status_, fromUtf8(status).c_str());
        RECT client{};
        GetClientRect(window_, &client);
        layoutChildren(client.right - client.left, client.bottom - client.top);
        SetFocus(editor_);
    }

    static bool isIdentifierByte(unsigned char character) {
        return std::isalnum(character) != 0 || character == '_';
    }

    void onEditorCharacterAdded(int character) {
        if (!editor_)
            return;
        const auto position = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
        if (character == '(') {
            const auto source = editorUtf8Source();
            if (position == 0 || position > source.size() ||
                source[position - 1] != '(')
                return;
            const std::size_t end = position - 1;
            std::size_t start = end;
            while (start > 0 &&
                   isIdentifierByte(static_cast<unsigned char>(source[start - 1])))
                --start;
            if (start == end)
                return;
            const auto name = source.substr(start, end - start);
            const char* callTip = nullptr;
            if (name == "sine")
                callTip = "sine(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nEmbedded sine voice and player controls";
            else if (name == "triangle")
                callTip = "triangle(notes, dur=1, oct=5, sus=1, amp=1, pan=0, strum=0)\nFoxDot-style triangle player";
            else if (name == "saw")
                callTip = "saw(notes, dur=1, oct=5, sus=1, amp=1, pan=0, cutoff=20000, resonance=0.2, strum=0)";
            else if (name == "pulse")
                callTip = "pulse(notes, dur=1, oct=5, sus=1, amp=1, pan=0, width=0.5, strum=0)";
            else if (name == "bass")
                callTip = "bass(notes, dur=1, oct=5, sus=1, rate=8, amp=1, pan=0)\nEmbedded FoxDot bass SynthDef";
            else if (name == "varsaw")
                callTip = "varsaw(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nrate sets pulse width from 0 to 1";
            else if (name == "dirt")
                callTip = "dirt(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nFoxDot dirt oscillator blend";
            else if (name == "jbass")
                callTip = "jbass(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nFoxDot sub-bass triangle";
            else if (name == "noise")
                callTip = "noise(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nPitched LFNoise0 voice";
            else if (name == "dub")
                callTip = "dub(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nFoxDot LFTri + SinOscFB patch";
            else if (name == "growl")
                callTip = "growl(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nFoxDot modulated sine multiplied by Saw";
            else if (name == "soprano")
                callTip = "soprano(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nVibrato plus SinOsc/SinOscFB";
            else if (name == "crunch")
                callTip = "crunch(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nCrackle-driven LFNoise0 texture";
            else if (name == "lazer")
                callTip = "lazer(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nFoxDot VarSaw/LFSaw/Pulse patch";
            else if (name == "rave")
                callTip = "rave(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nGendy1 uses source rate minus 1 for ampdist";
            else if (name == "bell")
                callTip = "bell(notes, dur=1, oct=5, sus=1, rate=1, amp=1, pan=0)\nKlank resonator bank with a triggered strike";
            else if (name == "gong")
                callTip = "gong(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nDetuned Klank gong with a high-pass";
            else if (name == "marimba")
                callTip = "marimba(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nPink-noise strike into a four-partial Klank";
            else if (name == "pluck")
                callTip = "pluck(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nDetuned sine pair with a triggered decay";
            else if (name == "twang")
                callTip = "twang(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nTriggered low-pass comb resonator";
            else if (name == "donk")
                callTip = "donk(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nTriggered pair of Ringz resonators";
            else if (name == "squish")
                callTip = "squish(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nPulse-fed Ringz resonator";
            else if (name == "charm")
                callTip = "charm(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\nFoxDot dual-oscillator resonant sweep";
            else if (name == "spark")
                callTip = "spark(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nFoxDot detuned saw texture with a fast rise and decay";
            else if (name == "blip")
                callTip = "blip(notes, dur=1, oct=5, sus=1, rate=8, amp=1, pan=0)\nFoxDot LFCub/LFTri/Blip oscillator";
            else if (name == "scatter")
                callTip = "scatter(notes, dur=1, oct=5, sus=1, rate=1, amp=1, pan=0)\nFoxDot saw/VarSaw texture with LFNoise0 modulation";
            else if (name == "orient")
                callTip = "orient(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nTwo layered LFPulse oscillators";
            else if (name == "zap")
                callTip = "zap(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nDetuned saw and VarSaw with noise motion";
            else if (name == "fuzz")
                callTip = "fuzz(notes, dur=1, oct=5, sus=1, amp=1, pan=0)\nNested LFSaw frequency modulation";
            else if (name == "bug")
                callTip = "bug(notes, dur=1, oct=5, sus=1, rate=1, amp=1, pan=0)\nPulse-width cluster modulated by SinOsc";
            else if (name == "ripple")
                callTip = "ripple(notes, dur=1, oct=5, sus=1, rate=1, amp=1, pan=0)\nLayered Pulse with slow amplitude motion";
            else if (name == "viola" || name == "nylon" || name == "arpy" ||
                     name == "dab" || name == "sawbass" || name == "dbass" ||
                     name == "prophet" || name == "pads" || name == "pasha" ||
                     name == "ambi" || name == "sinepad" || name == "keys" ||
                     name == "klank" || name == "feel" || name == "glass" ||
                     name == "soft" || name == "quin" || name == "space" ||
                     name == "scratch" || name == "creep" || name == "snick" ||
                     name == "swell" || name == "razz" || name == "sitar" ||
                     name == "star" || name == "karp")
                callTip = "synth(notes, dur=1, oct=5, sus=1, rate=0, amp=1, pan=0)\n"
                          "Original FoxDot core-UGen graph; see its Portable example.\n"
                          "Scalar controls: vib, depth, lforate, lfowidth, rq, fmod, blur";
            else if (name == "var")
                callTip = "var(values, durations=4)\nCycle values using FoxDot-style beat durations";
            else if (name == "linvar")
                callTip = "linvar(values, durations=4)\nLinearly interpolate between values";
            else if (name == "PGroup")
                callTip = "PGroup(*notes)\nGroup notes for chord and pattern operations";
            else if (name == "play")
                callTip = "play(\"x-o-\", dur=0.5, sample=0, rate=1, amp=1, pan=0)\n"
                          "Portable WAVs; - is a hat, spaces are rests; negative rate reverses.\n"
                          "Legacy: play(notes, step=1, duration=0.45, velocity=100, strum=0)";
            else if (name == "set")
                callTip = "set(value)\nSet the default root or scale";
            if (callTip)
                SendMessageW(editor_, SCI_CALLTIPSHOW, position,
                             reinterpret_cast<LPARAM>(callTip));
            return;
        }

        if (!isIdentifierByte(static_cast<unsigned char>(character)))
            return;
        const auto source = editorUtf8Source();
        if (position > source.size())
            return;
        std::size_t start = position;
        while (start > 0 &&
               isIdentifierByte(static_cast<unsigned char>(source[start - 1])))
            --start;
        const auto prefixLength = position - start;
        if (prefixLength >= 2)
            SendMessageW(editor_, SCI_AUTOCSHOW, prefixLength,
                         reinterpret_cast<LPARAM>(kFoxDotCompletions));
    }

    int selectedPanelTab() const {
        return panelTabs_ ? TabCtrl_GetCurSel(panelTabs_) : 0;
    }

    void onPanelTabChanged() {
        panelPlayersInitialized_ = false;
        panelEffectsInitialized_ = false;
        panelSynthsInitialized_ = false;
        renderPanelTab();
    }

    static bool samePlayerStates(
        const std::array<EditorPlayerState, 8>& left,
        const std::array<EditorPlayerState, 8>& right) {
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (left[index].active != right[index].active ||
                left[index].muted != right[index].muted ||
                left[index].effectMixAvailable !=
                    right[index].effectMixAvailable ||
                left[index].synthIndex != right[index].synthIndex ||
                left[index].effectIndex != right[index].effectIndex ||
                left[index].effectIndices != right[index].effectIndices ||
                left[index].reverbIndex != right[index].reverbIndex ||
                left[index].echoEnabled != right[index].echoEnabled ||
                left[index].reverbEnabled != right[index].reverbEnabled ||
                left[index].shapeEnabled != right[index].shapeEnabled ||
                left[index].driveEnabled != right[index].driveEnabled ||
                left[index].chopEnabled != right[index].chopEnabled ||
                left[index].spinEnabled != right[index].spinEnabled ||
                left[index].coarseEnabled != right[index].coarseEnabled ||
                left[index].formantEnabled != right[index].formantEnabled ||
                left[index].pshiftEnabled != right[index].pshiftEnabled ||
                left[index].bitcrusherEnabled != right[index].bitcrusherEnabled ||
                left[index].bits != right[index].bits ||
                left[index].crush != right[index].crush ||
                left[index].pshift != right[index].pshift)
                return false;
            for (std::size_t slot = 0;
                 slot < left[index].effectControlValues.size(); ++slot)
                if (left[index].effectControlValues[slot] !=
                    right[index].effectControlValues[slot])
                    return false;
        }
        return true;
    }

    static int panelItemPlayer(int itemData) {
        if (itemData >= 0 && itemData < 8)
            return itemData;
        if (itemData >= kEffectChainItemDataBase &&
            itemData < kEffectChainItemDataBase + 8 * 4)
            return (itemData - kEffectChainItemDataBase) / 4;
        if (itemData >= kEffectChainHeaderDataBase &&
            itemData < kEffectChainHeaderDataBase + 8)
            return itemData - kEffectChainHeaderDataBase;
        return -1;
    }

    static int panelItemEffectSlot(int itemData) {
        if (itemData < kEffectChainItemDataBase ||
            itemData >= kEffectChainItemDataBase + 8 * 4)
            return -1;
        return (itemData - kEffectChainItemDataBase) % 4;
    }

    static int panelItemForEffect(int player, int slot) {
        return player >= 0 && player < 8 && slot >= 0 && slot < 4
            ? kEffectChainItemDataBase + player * 4 + slot : -1;
    }

    static int panelItemForChainHeader(int player) {
        return player >= 0 && player < 8
            ? kEffectChainHeaderDataBase + player : -1;
    }

    void addPanelRow(const std::wstring& row, int itemData = -1) {
        if (!panelList_)
            return;
        const auto index = static_cast<int>(
            SendMessageW(panelList_, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(row.c_str())));
        if (index != LB_ERR)
            SendMessageW(panelList_, LB_SETITEMDATA, index, itemData);
    }

    struct PanelScrollAnchor {
        int topIndex = 0;
        int matchingLabelOccurrence = 0;
        std::wstring label;
        bool hasLabel = false;
    };

    std::wstring panelRowText(int row) const {
        if (!panelList_ || row < 0)
            return {};
        const auto length = static_cast<int>(
            SendMessageW(panelList_, LB_GETTEXTLEN, row, 0));
        if (length == LB_ERR || length <= 0)
            return {};
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        const auto copied = static_cast<int>(SendMessageW(
            panelList_, LB_GETTEXT, row,
            reinterpret_cast<LPARAM>(text.data())));
        if (copied == LB_ERR)
            return {};
        text.resize(static_cast<std::size_t>(copied));
        return text;
    }

    PanelScrollAnchor capturePanelScrollAnchor() const {
        PanelScrollAnchor anchor;
        if (!panelList_)
            return anchor;
        anchor.topIndex = static_cast<int>(
            SendMessageW(panelList_, LB_GETTOPINDEX, 0, 0));
        if (anchor.topIndex == LB_ERR || anchor.topIndex < 0)
            anchor.topIndex = 0;
        anchor.label = panelRowText(anchor.topIndex);
        if (anchor.label.empty())
            return anchor;
        anchor.hasLabel = true;
        const int count = static_cast<int>(
            SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
        for (int row = 0; row <= anchor.topIndex && row < count; ++row)
            if (panelRowText(row) == anchor.label)
                ++anchor.matchingLabelOccurrence;
        --anchor.matchingLabelOccurrence;
        return anchor;
    }

    void restorePanelScrollAnchor(const PanelScrollAnchor& anchor) const {
        if (!panelList_)
            return;
        const int count = static_cast<int>(
            SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
        if (count <= 0 || count == LB_ERR)
            return;
        int target = std::clamp(anchor.topIndex, 0, count - 1);
        if (anchor.hasLabel) {
            int occurrence = 0;
            for (int row = 0; row < count; ++row) {
                if (panelRowText(row) != anchor.label)
                    continue;
                if (occurrence++ == anchor.matchingLabelOccurrence) {
                    target = row;
                    break;
                }
            }
        }
        SendMessageW(panelList_, LB_SETTOPINDEX, target, 0);
    }

    std::string catalogCategoryKey(const std::string& category) const {
        return std::to_string(selectedPanelTab()) + ":" + category;
    }

    bool catalogCategoryCollapsed(const std::string& category) {
        const auto key = catalogCategoryKey(category);
        auto state = collapsedCatalogCategories_.find(key);
        if (state == collapsedCatalogCategories_.end())
            state = collapsedCatalogCategories_.emplace(key, true).first;
        return state->second;
    }

    std::string catalogSubcategoryKey(const std::string& category,
                                      const std::string& subcategory) const {
        return catalogCategoryKey(category) + ":" + subcategory;
    }

    bool catalogSubcategoryCollapsed(const std::string& category,
                                     const std::string& subcategory) {
        const auto key = catalogSubcategoryKey(category, subcategory);
        auto state = collapsedCatalogSubcategories_.find(key);
        if (state == collapsedCatalogSubcategories_.end())
            state = collapsedCatalogSubcategories_.emplace(key, true).first;
        return state->second;
    }

    void onPanelListSelectionChanged() {
        if (!panelList_) {
            updatePlayerActionButtons();
            return;
        }
        const int selected = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selected != LB_ERR) {
            const auto itemData = static_cast<int>(
                SendMessageW(panelList_, LB_GETITEMDATA, selected, 0));
            if (selectedPanelTab() == 2 &&
                itemData == kActiveEffectSummaryHeaderData) {
                const auto scrollAnchor = capturePanelScrollAnchor();
                activeEffectSummaryExpanded_ =
                    !activeEffectSummaryExpanded_;
                renderPanelTab();
                const int count = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                for (int row = 0; row < count; ++row) {
                    if (static_cast<int>(SendMessageW(
                            panelList_, LB_GETITEMDATA, row, 0)) !=
                        kActiveEffectSummaryHeaderData)
                        continue;
                    SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                    break;
                }
                restorePanelScrollAnchor(scrollAnchor);
                updatePlayerActionButtons();
                return;
            }
            const int headerPlayer =
                itemData >= kEffectChainHeaderDataBase &&
                        itemData < kEffectChainHeaderDataBase + 8
                    ? itemData - kEffectChainHeaderDataBase : -1;
            if (headerPlayer >= 0) {
                const auto scrollAnchor = capturePanelScrollAnchor();
                const bool expanding =
                    !expandedEffectChains_[static_cast<std::size_t>(
                        headerPlayer)];
                expandedEffectChains_[static_cast<std::size_t>(headerPlayer)] =
                    expanding;
                panelPlayersInitialized_ = false;
                renderPanelTab();
                int targetItemData = headerPlayer;
                if (expanding) {
                    const int firstSlot =
                        dexter::editor::firstConfiguredEffectSlot(
                            lastPanelPlayers_[static_cast<std::size_t>(
                                headerPlayer)].effectIndices);
                    if (firstSlot >= 0)
                        targetItemData =
                            panelItemForEffect(headerPlayer, firstSlot);
                }
                const int count = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                for (int row = 0; row < count; ++row) {
                    if (static_cast<int>(SendMessageW(
                            panelList_, LB_GETITEMDATA, row, 0)) !=
                        targetItemData)
                        continue;
                    SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                    break;
                }
                restorePanelScrollAnchor(scrollAnchor);
                updatePlayerActionButtons();
                return;
            }
        }
        const auto subcategoryHeader =
            catalogSubcategoryHeaderRows_.find(selected);
        if (selected != LB_ERR &&
            subcategoryHeader != catalogSubcategoryHeaderRows_.end()) {
            const auto group = subcategoryHeader->second;
            const auto key = catalogSubcategoryKey(group.first, group.second);
            const bool collapsed =
                catalogSubcategoryCollapsed(group.first, group.second);
            const auto scrollAnchor = capturePanelScrollAnchor();
            collapsedCatalogSubcategories_[key] = !collapsed;
            renderPanelTab();
            for (const auto& row : catalogSubcategoryHeaderRows_) {
                if (row.second == group) {
                    SendMessageW(panelList_, LB_SETCURSEL, row.first, 0);
                    break;
                }
            }
            restorePanelScrollAnchor(scrollAnchor);
            updatePlayerActionButtons();
            return;
        }
        const auto header = catalogCategoryHeaderRows_.find(selected);
        if (selected != LB_ERR && header != catalogCategoryHeaderRows_.end()) {
            const auto category = header->second;
            const auto key = catalogCategoryKey(category);
            const bool collapsed = catalogCategoryCollapsed(category);
            const auto scrollAnchor = capturePanelScrollAnchor();
            collapsedCatalogCategories_[key] = !collapsed;
            renderPanelTab();
            for (const auto& row : catalogCategoryHeaderRows_) {
                if (row.second == category) {
                    SendMessageW(panelList_, LB_SETCURSEL, row.first, 0);
                    break;
                }
            }
            restorePanelScrollAnchor(scrollAnchor);
        }
        updatePlayerActionButtons();
    }

    void showCatalogAbout(const dexter::editor::CatalogEntry& entry) {
        const auto directory = portableScriptDirectory();
        if (directory.empty()) {
            const auto message = fromUtf8(entry.label + "\n\n" + entry.about);
            MessageBoxW(window_, message.c_str(),
                        L"Instrument / effect information",
                        MB_OK | MB_ICONINFORMATION);
            return;
        }
        const auto help = directory / L"179-editor-catalog-about.html";
        std::array<wchar_t, 32768> pageUrl{};
        DWORD pageUrlLength = static_cast<DWORD>(pageUrl.size());
        const auto pathResult = UrlCreateFromPathW(
            help.c_str(), pageUrl.data(), &pageUrlLength, 0);
        std::array<wchar_t, 1024> escapedAnchor{};
        DWORD escapedLength = static_cast<DWORD>(escapedAnchor.size());
        const auto anchor = fromUtf8(entry.name);
        const auto anchorResult = UrlEscapeW(
            anchor.c_str(), escapedAnchor.data(), &escapedLength,
            URL_ESCAPE_SEGMENT_ONLY);
        std::wstring target;
        if (SUCCEEDED(pathResult) && SUCCEEDED(anchorResult)) {
            target.assign(pageUrl.data());
            target += L"#";
            target += escapedAnchor.data();
        }
        const auto opened = target.empty() ? 0 : reinterpret_cast<INT_PTR>(
            ShellExecuteW(window_, L"open", target.c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL));
        if (opened > 32)
            return;
        const auto message = fromUtf8(entry.label + "\n\n" + entry.about);
        MessageBoxW(window_, message.c_str(), L"Instrument / effect information",
                    MB_OK | MB_ICONINFORMATION);
    }

    void insertCatalogEntryAtCaret(
        const dexter::editor::CatalogEntry& entry,
        std::size_t position) {
        if (activeTab_ != 0 || !entry.available)
            return;
        auto source = editorUtf8Source();
        std::string failure;
        if (!dexter::editor::applyCatalogEntry(source, position, entry,
                                                failure)) {
            SetWindowTextW(status_, fromUtf8(failure).c_str());
            return;
        }
        applyEditorTextEdit(source);
        SetWindowTextW(status_, L"Added to script. Click Run to apply it.");
    }

    bool showPanelCatalogMenu(LPARAM coordinates) {
        if (!panelList_)
            return false;
        int row = LB_ERR;
        const POINTS packed = MAKEPOINTS(coordinates);
        if (packed.x == -1 && packed.y == -1) {
            row = static_cast<int>(SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        } else {
            POINT local{packed.x, packed.y};
            if (!ScreenToClient(panelList_, &local))
                return false;
            const auto hit = static_cast<DWORD>(SendMessageW(
                panelList_, LB_ITEMFROMPOINT, 0,
                MAKELPARAM(local.x, local.y)));
            if (HIWORD(hit) == 0)
                row = LOWORD(hit);
        }
        if (row < 0)
            return false;
        const auto itemData = static_cast<LRESULT>(
            SendMessageW(panelList_, LB_GETITEMDATA, row, 0));
        if (itemData < 0 ||
            static_cast<std::size_t>(itemData) >= panelCatalog_.size())
            return false;
        const auto& entry = panelCatalog_[static_cast<std::size_t>(itemData)];
        HMENU menu = CreatePopupMenu();
        if (!menu)
            return true;
        AppendMenuW(menu, MF_STRING |
                    (entry.available && activeTab_ == 0 ? 0 : MF_GRAYED),
                    kContextPanelInsertId, L"Insert at editor caret");
        AppendMenuW(menu, MF_STRING, kContextPanelAboutId,
                    L"About this item");
        POINT point{packed.x, packed.y};
        if (packed.x == -1 && packed.y == -1) {
            RECT itemRect{};
            SendMessageW(panelList_, LB_GETITEMRECT, row,
                         reinterpret_cast<LPARAM>(&itemRect));
            point.x = itemRect.left;
            point.y = itemRect.bottom;
            ClientToScreen(panelList_, &point);
        }
        const auto command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0,
            window_, nullptr);
        DestroyMenu(menu);
        if (command == kContextPanelAboutId) {
            showCatalogAbout(entry);
        } else if (command == kContextPanelInsertId && editor_) {
            const auto caret = static_cast<std::size_t>(
                SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
            insertCatalogEntryAtCaret(entry, caret);
        }
        return true;
    }

    void beginCatalogDrag(LPARAM coordinates) {
        resetCatalogDrag();
        if (activeTab_ != 0)
            return;
        const auto hit = static_cast<DWORD>(SendMessageW(
            panelList_, LB_ITEMFROMPOINT, 0, coordinates));
        if (HIWORD(hit) != 0)
            return;
        const auto row = static_cast<int>(LOWORD(hit));
        SendMessageW(panelList_, LB_SETCURSEL, row, 0);
        const auto itemData = static_cast<LRESULT>(
            SendMessageW(panelList_, LB_GETITEMDATA, row, 0));
        if (itemData < 0 ||
            static_cast<std::size_t>(itemData) >= panelCatalog_.size())
            return;
        const auto& entry = panelCatalog_[static_cast<std::size_t>(itemData)];
        if (!entry.available)
            return;
        dragCatalogEntry_ = entry;
        dragCatalogPending_ = true;
        GetCursorPos(&catalogDragOrigin_);
    }

    bool updateCatalogDrag(WPARAM buttons, LPARAM) {
        if (!dragCatalogPending_ || !(buttons & MK_LBUTTON))
            return false;
        POINT cursor{};
        GetCursorPos(&cursor);
        if (!catalogDragActive_ &&
            std::abs(cursor.x - catalogDragOrigin_.x) < GetSystemMetrics(SM_CXDRAG) &&
            std::abs(cursor.y - catalogDragOrigin_.y) < GetSystemMetrics(SM_CYDRAG))
            return false;
        if (!catalogDragActive_) {
            catalogDragActive_ = true;
            SetCapture(panelList_);
            SetWindowTextW(status_, L"Drop on a pN >> player line to insert.");
        }
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return true;
    }

    void resetCatalogDrag() {
        dragCatalogPending_ = false;
        catalogDragActive_ = false;
    }

    bool finishCatalogDrag(LPARAM) {
        if (!dragCatalogPending_ && !catalogDragActive_)
            return false;
        const bool wasActive = catalogDragActive_;
        const auto entry = dragCatalogEntry_;
        resetCatalogDrag();
        if (GetCapture() == panelList_)
            ReleaseCapture();
        if (!wasActive || !editor_ || activeTab_ != 0)
            return wasActive;
        POINT cursor{};
        GetCursorPos(&cursor);
        POINT client = cursor;
        if (!ScreenToClient(editor_, &client))
            return true;
        RECT bounds{};
        GetClientRect(editor_, &bounds);
        if (!PtInRect(&bounds, client)) {
            SetWindowTextW(status_, L"Drop canceled. Drag onto a player line.");
            return true;
        }
        const auto position = static_cast<std::size_t>(SendMessageW(
            editor_, kSciPositionFromPointClose, client.x, client.y));
        insertCatalogEntryAtCaret(entry, position);
        return true;
    }

    std::wstring synthNameForIndex(
        int synthIndex, const std::vector<std::string>& externalNames) const {
        if (synthIndex >= 0 &&
            synthIndex < static_cast<int>(kEmbeddedSynthNames.size()))
            return fromUtf8(kEmbeddedSynthNames[static_cast<std::size_t>(synthIndex)]);
        const int externalIndex =
            synthIndex - static_cast<int>(kEmbeddedSynthNames.size());
        if (externalIndex >= 0 &&
            externalIndex < static_cast<int>(externalNames.size()))
            return fromUtf8(externalNames[static_cast<std::size_t>(externalIndex)]);
        return L"unknown";
    }

    void renderPanelTab() {
        if (!panelList_)
            return;
        const int tab = selectedPanelTab();
        const auto scrollAnchor = renderedPanelTab_ == tab
            ? capturePanelScrollAnchor() : PanelScrollAnchor{};
        SendMessageW(panelList_, LB_RESETCONTENT, 0, 0);
        catalogCategoryHeaderRows_.clear();
        catalogSubcategoryHeaderRows_.clear();
        ShowWindow(panelMuteButton_, tab == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(panelStopButton_, tab == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(panelSamplesButton_, tab == 3 ? SW_SHOW : SW_HIDE);
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            const int visibility = tab == 0 ? SW_SHOW : SW_HIDE;
            ShowWindow(playerControlLabels_[index], visibility);
            ShowWindow(playerControlSliders_[index], visibility);
            ShowWindow(playerControlValues_[index], visibility);
            ShowWindow(playerControlRanges_[index], SW_HIDE);
        }
        if (tab == 0) {
            panelPlayersInitialized_ = false;
            panelCatalog_.clear();
            refreshPlayersPanel();
        } else if (tab == 1) {
            const auto externalNames =
                host_ ? host_->editorExternalSynthNames()
                      : std::vector<std::string>{};
            panelCatalog_ = dexter::editor::makeInstrumentCatalog(
                std::vector<std::string>(kEmbeddedSynthNames.begin(),
                                         kEmbeddedSynthNames.end()),
                externalNames);
            renderCatalogEntries();
            if (host_) {
                lastPanelSynthNames_ = externalNames;
                panelSynthsInitialized_ = true;
            }
        } else if (tab == 2) {
            const auto externalNames =
                host_ ? host_->editorExternalSynthNames()
                      : std::vector<std::string>{};
            renderLiveEffectsPanel();
            panelCatalog_ =
                dexter::editor::makeEffectCatalog(externalNames);
            renderCatalogEntries();
        } else if (tab == 3) {
            panelCatalog_.clear();
            renderSamplePanel();
        }
        renderedPanelTab_ = tab;
        restorePanelScrollAnchor(scrollAnchor);
        updatePlayerActionButtons();
    }

    void renderCatalogEntries() {
        std::string currentCategory;
        std::string currentSubcategory;
        for (std::size_t index = 0; index < panelCatalog_.size(); ++index) {
            const auto& entry = panelCatalog_[index];
            if (entry.category != currentCategory) {
                currentCategory = entry.category;
                currentSubcategory.clear();
                const bool collapsed =
                    catalogCategoryCollapsed(currentCategory);
                const int row = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                addPanelRow((collapsed ? L"[+] " : L"[-] ") +
                            fromUtf8(currentCategory));
                catalogCategoryHeaderRows_[row] = currentCategory;
            }
            if (catalogCategoryCollapsed(currentCategory))
                continue;
            if (entry.subcategory != currentSubcategory) {
                currentSubcategory = entry.subcategory;
                const bool collapsed = catalogSubcategoryCollapsed(
                    currentCategory, currentSubcategory);
                const int row = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                addPanelRow(std::wstring(L"    ") +
                            (collapsed ? L"[+] " : L"[-] ") +
                            fromUtf8(currentSubcategory));
                catalogSubcategoryHeaderRows_[row] =
                    {currentCategory, currentSubcategory};
            }
            if (catalogSubcategoryCollapsed(currentCategory,
                                            currentSubcategory))
                continue;
            const auto label = entry.available
                ? entry.label : entry.label + " [not loaded]";
            addPanelRow(std::wstring(L"        ") + fromUtf8(label),
                        static_cast<int>(index));
        }
    }

    void renderLiveEffectsPanel() {
        if (!host_)
            return;
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        lastPanelPlayers_ = states;
        panelEffectsInitialized_ = true;
        const auto externalNames = host_->editorExternalSynthNames();
        static constexpr std::array<const wchar_t*, 4> reverbs{
            L"none", L"Freeverb", L"Freeverb2", L"GVerb"};
        std::array<std::wstring, 8> activeRows{};
        std::size_t activeCount = 0;
        for (std::size_t index = 0; index < states.size(); ++index) {
            const auto& state = states[index];
            if (state.effectIndex < 0 && !state.echoEnabled &&
                !state.reverbEnabled && !state.shapeEnabled &&
                !state.driveEnabled && !state.chopEnabled &&
                !state.spinEnabled && !state.coarseEnabled &&
                !state.formantEnabled && !state.pshiftEnabled &&
                !state.bitcrusherEnabled)
                continue;
            std::wstring row = L"p" + std::to_wstring(index + 1) + L":";
            bool firstInsert = true;
            for (const auto effectIndex : state.effectIndices) {
                if (effectIndex >= 0 &&
                    effectIndex < static_cast<int>(externalNames.size())) {
                    row += firstInsert ? L" " : L" -> ";
                    row += fromUtf8(
                        externalNames[static_cast<std::size_t>(effectIndex)]);
                    firstInsert = false;
                }
            }
            if (state.echoEnabled)
                row += L" Echo";
            if (state.shapeEnabled)
                row += L" Shape";
            if (state.driveEnabled)
                row += L" Drive";
            if (state.chopEnabled)
                row += L" Chop";
            if (state.spinEnabled)
                row += L" Spin";
            if (state.coarseEnabled)
                row += L" Coarse";
            if (state.formantEnabled)
                row += L" Formant";
            if (state.pshiftEnabled)
                row += L" PShift";
            if (state.bitcrusherEnabled)
                row += L" Bits/Crush";
            if (state.reverbEnabled && state.reverbIndex >= 0 &&
                state.reverbIndex < static_cast<int>(reverbs.size()))
                row += L" " + std::wstring(
                    reverbs[static_cast<std::size_t>(state.reverbIndex)]);
            if (!state.active)
                row += L" [tail/idle]";
            activeRows[activeCount++] = std::move(row);
        }
        if (activeCount == 0)
            return;
        addPanelRow(std::wstring(activeEffectSummaryExpanded_
                                     ? L"[-] Active player effect status ("
                                     : L"[+] Active player effect status (") +
                    std::to_wstring(activeCount) + L")",
                    kActiveEffectSummaryHeaderData);
        if (!activeEffectSummaryExpanded_)
            return;
        for (std::size_t index = 0; index < activeCount; ++index)
            addPanelRow(std::wstring(L"    ") + activeRows[index]);
    }

    void refreshEffectsPanel() {
        if (!host_ || selectedPanelTab() != 2)
            return;
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        if (!panelEffectsInitialized_ ||
            !samePlayerStates(states, lastPanelPlayers_))
            renderPanelTab();
    }

    void refreshSynthCatalog() {
        if (!host_ || selectedPanelTab() != 1)
            return;
        const auto names = host_->editorExternalSynthNames();
        if (!panelSynthsInitialized_ || names != lastPanelSynthNames_)
            renderPanelTab();
    }

    static bool isWaveFile(const std::filesystem::path& path) {
        auto extension = path.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](wchar_t character) {
                           return static_cast<wchar_t>(std::towlower(character));
                       });
        return extension == L".wav" || extension == L".wave";
    }

    void appendSampleBank(const std::filesystem::path& folder,
                          const std::wstring& symbol,
                          std::vector<SampleBrowserEntry>& entries,
                          std::size_t& displayedFiles) {
        std::error_code error;
        if (!std::filesystem::is_directory(folder, error) || error)
            return;
        std::vector<std::filesystem::path> waves;
        for (std::filesystem::directory_iterator it(folder, error), end;
             !error && it != end && displayedFiles < 256; it.increment(error)) {
            if (!it->is_regular_file(error) || it->is_symlink(error) ||
                !isWaveFile(it->path()))
                continue;
            waves.push_back(it->path());
        }
        if (error || waves.empty())
            return;
        std::sort(waves.begin(), waves.end());
        for (std::size_t index = 0;
             index < waves.size() && displayedFiles < 256; ++index) {
            const auto filename = waves[index].filename().u8string();
            const auto categories =
                dexter::editor::sampleCategoryForFilename(filename);
            entries.push_back({
                categories.first, categories.second,
                dexter::editor::catalogLower(filename),
                symbol + L"[" + std::to_wstring(index) + L"]  " +
                    fromUtf8(filename)});
            ++displayedFiles;
        }
    }

    void renderSamplePanel() {
        const auto root = portableScriptDirectory() / L"Samples";
        addPanelRow(L"One row per sample: symbol[index] and WAV filename.");
        addPanelRow(L"x-o- uses x/o letter banks; spaces are rests.");
        addPanelRow(L"sample=1 picks bank item 1; |x1| changes one hit.");
        addPanelRow(L"Virtual groups use filenames; files and sample indices stay put.");
        addPanelRow(L"Use new filenames, then Run; replacing same file needs host restart.");
        std::size_t displayedFiles = 0;
        std::vector<SampleBrowserEntry> entries;
        std::error_code error;
        for (wchar_t letter = L'a'; letter <= L'z' && displayedFiles < 256;
             ++letter) {
            const std::wstring symbol(1, letter);
            const auto bank = root / symbol;
            appendSampleBank(bank / L"lower", symbol + L" (lower)",
                             entries, displayedFiles);
            appendSampleBank(bank / L"upper", symbol + L" (upper)",
                             entries, displayedFiles);
            appendSampleBank(bank, symbol, entries, displayedFiles);
        }
        const auto specialRoot = root / L"_";
        for (std::filesystem::directory_iterator it(specialRoot, error), end;
             !error && it != end && displayedFiles < 256; it.increment(error)) {
            if (!it->is_directory(error) || it->is_symlink(error))
                continue;
            appendSampleBank(it->path(), L"_" +
                it->path().filename().wstring(), entries, displayedFiles);
        }
        if (entries.empty())
            addPanelRow(L"No WAV banks yet. Open the folder to add some.");
        else if (displayedFiles >= 256)
            addPanelRow(L"List capped at 256 WAVs; more may be installed.");
        std::sort(entries.begin(), entries.end(),
                  [](const auto& left, const auto& right) {
            return std::make_tuple(left.category, left.subcategory,
                                   left.sortKey, left.label) <
                   std::make_tuple(right.category, right.subcategory,
                                   right.sortKey, right.label);
        });
        std::string currentCategory;
        std::string currentSubcategory;
        for (const auto& entry : entries) {
            if (entry.category != currentCategory) {
                currentCategory = entry.category;
                currentSubcategory.clear();
                const bool collapsed =
                    catalogCategoryCollapsed(currentCategory);
                const int row = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                addPanelRow((collapsed ? L"[+] " : L"[-] ") +
                            fromUtf8(currentCategory));
                catalogCategoryHeaderRows_[row] = currentCategory;
            }
            if (catalogCategoryCollapsed(currentCategory))
                continue;
            if (entry.subcategory != currentSubcategory) {
                currentSubcategory = entry.subcategory;
                const bool collapsed = catalogSubcategoryCollapsed(
                    currentCategory, currentSubcategory);
                const int row = static_cast<int>(
                    SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
                addPanelRow(std::wstring(L"    ") +
                            (collapsed ? L"[+] " : L"[-] ") +
                            fromUtf8(currentSubcategory));
                catalogSubcategoryHeaderRows_[row] =
                    {currentCategory, currentSubcategory};
            }
            if (catalogSubcategoryCollapsed(currentCategory,
                                            currentSubcategory))
                continue;
            addPanelRow(std::wstring(L"        ") + entry.label);
        }
    }

    void refreshPlayersPanel() {
        if (!host_ || !panelList_ || selectedPanelTab() != 0)
            return;
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        if (panelPlayersInitialized_ &&
            samePlayerStates(states, lastPanelPlayers_)) {
            updatePlayerControlSliders(states);
            return;
        }
        const auto scrollAnchor = capturePanelScrollAnchor();
        int selectedItemData = -1;
        const int selectedItem = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selectedItem != LB_ERR)
            selectedItemData = static_cast<int>(
                SendMessageW(panelList_, LB_GETITEMDATA, selectedItem, 0));
        SendMessageW(panelList_, LB_RESETCONTENT, 0, 0);
        const auto externalNames = host_->editorExternalSynthNames();
        static constexpr std::array<const wchar_t*, 4> reverbs{
            L"none", L"Freeverb", L"Freeverb2", L"GVerb"};
        for (std::size_t index = 0; index < states.size(); ++index) {
            const auto& state = states[index];
            std::wstring row = L"p" + std::to_wstring(index + 1) +
                (state.active ? L"  PLAY  " : L"  stopped  ") +
                synthNameForIndex(state.synthIndex, externalNames);
            if (state.muted)
                row += L"  [MUTED]";
            if (state.effectIndex >= 0 &&
                state.effectIndex < static_cast<int>(externalNames.size())) {
                row += L"  FX:" +
                    fromUtf8(externalNames[static_cast<std::size_t>(state.effectIndex)]);
            }
            if (state.echoEnabled)
                row += L"  Echo";
            if (state.reverbEnabled && state.reverbIndex >= 0 &&
                state.reverbIndex < static_cast<int>(reverbs.size()))
                row += L"  " + std::wstring(reverbs[static_cast<std::size_t>(
                    state.reverbIndex)]);
            if (state.shapeEnabled)
                row += L"  Shape";
            if (state.driveEnabled)
                row += L"  Drive";
            if (state.chopEnabled)
                row += L"  Chop";
            if (state.spinEnabled)
                row += L"  Spin";
            if (state.coarseEnabled)
                row += L"  Coarse";
            if (state.formantEnabled)
                row += L"  Formant";
            if (state.pshiftEnabled)
                row += L"  PShift";
            if (state.bitcrusherEnabled)
                row += L"  Bits/Crush";
            addPanelRow(row, static_cast<int>(index));
            std::size_t effectCount = 0;
            for (const auto effectIndex : state.effectIndices)
                effectCount += effectIndex >= 0 ? 1u : 0u;
            if (effectCount == 0)
                continue;
            const bool expanded = expandedEffectChains_[index];
            addPanelRow(
                std::wstring(L"    ") + (expanded ? L"[-] " : L"[+] ") +
                    L"Effect chain (" + std::to_wstring(effectCount) + L")",
                panelItemForChainHeader(static_cast<int>(index)));
            if (!expanded)
                continue;
            for (std::size_t effectSlot = 0;
                 effectSlot < state.effectIndices.size(); ++effectSlot) {
                const auto effectIndex = state.effectIndices[effectSlot];
                if (effectIndex < 0 ||
                    effectIndex >= static_cast<int>(externalNames.size()))
                    continue;
                std::wstring effectRow = L"        ";
                effectRow += selectedItemData == panelItemForEffect(
                    static_cast<int>(index),
                    static_cast<int>(effectSlot)) ? L"[-] " : L"[+] ";
                effectRow += L"#" + std::to_wstring(effectSlot + 1) + L" ";
                effectRow += fromUtf8(externalNames[
                    static_cast<std::size_t>(effectIndex)]);
                addPanelRow(effectRow, panelItemForEffect(
                    static_cast<int>(index), static_cast<int>(effectSlot)));
            }
        }
        lastPanelPlayers_ = states;
        panelPlayersInitialized_ = true;
        bool restoredSelection = false;
        if (selectedItemData >= 0) {
            const int count = static_cast<int>(
                SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
            for (int row = 0; row < count; ++row) {
                if (static_cast<int>(SendMessageW(
                        panelList_, LB_GETITEMDATA, row, 0)) !=
                    selectedItemData)
                    continue;
                SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                restoredSelection = true;
                break;
            }
        }
        if (!restoredSelection) {
            for (std::size_t index = 0; index < states.size(); ++index) {
                if (!states[index].active)
                    continue;
                SendMessageW(panelList_, LB_SETCURSEL,
                             static_cast<WPARAM>(index), 0);
                break;
            }
        }
        restorePanelScrollAnchor(scrollAnchor);
        updatePlayerActionButtons();
    }

    int selectedPlayerIndex() const {
        if (!panelList_ || selectedPanelTab() != 0)
            return -1;
        const int selectedItem = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selectedItem == LB_ERR)
            return -1;
        const int itemData = static_cast<int>(
            SendMessageW(panelList_, LB_GETITEMDATA, selectedItem, 0));
        return panelItemPlayer(itemData);
    }

    int selectedEffectSlotIndex() const {
        if (!panelList_ || selectedPanelTab() != 0)
            return -1;
        const int selectedItem = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selectedItem == LB_ERR)
            return -1;
        const int itemData = static_cast<int>(
            SendMessageW(panelList_, LB_GETITEMDATA, selectedItem, 0));
        return panelItemEffectSlot(itemData);
    }

    void selectPlayerAtEditorCaret() {
        if (!editor_ || !panelList_ || selectedPanelTab() != 0)
            return;
        const auto source = editorUtf8Source();
        const auto position = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentAt(
                source, position, assignment))
            return;
        const int count = static_cast<int>(
            SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
        for (int row = 0; row < count; ++row) {
            const auto player = static_cast<int>(
                SendMessageW(panelList_, LB_GETITEMDATA, row, 0));
            if (player != assignment.playerIndex)
                continue;
            if (SendMessageW(panelList_, LB_GETCURSEL, 0, 0) != row) {
                SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                updatePlayerActionButtons();
            }
            return;
        }
    }

    void updatePlayerActionButtons() {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_) {
            static constexpr std::array<const wchar_t*, 7> labels{
                L"Level", L"Pan", L"Cutoff", L"FX mix", L"Bits", L"Crush",
                L"PShift"};
            if (panelMuteButton_)
                EnableWindow(panelMuteButton_, FALSE);
            if (panelStopButton_)
                EnableWindow(panelStopButton_, FALSE);
            for (std::size_t index = 0;
                 index < playerControlSliders_.size(); ++index) {
                if (playerControlLabels_[index])
                    SetWindowTextW(playerControlLabels_[index], labels[index]);
                if (playerControlSliders_[index])
                    EnableWindow(playerControlSliders_[index], FALSE);
                const auto control = index == 0 ? EditorPlayerControl::Amplitude
                    : index == 1 ? EditorPlayerControl::Pan
                    : index == 2 ? EditorPlayerControl::Cutoff
                    : index == 3 ? EditorPlayerControl::EffectMix
                    : index == 4 ? EditorPlayerControl::BitDepth
                    : index == 5 ? EditorPlayerControl::CrushRate
                                 : EditorPlayerControl::PShift;
                const int visibility = editorPlayerControlVisible(
                    control, selectedPanelTab() == 0, false, false, false)
                    ? SW_SHOW : SW_HIDE;
                ShowWindow(playerControlLabels_[index], visibility);
                ShowWindow(playerControlSliders_[index], visibility);
                ShowWindow(playerControlValues_[index], visibility);
                ShowWindow(playerControlRanges_[index], SW_HIDE);
            }
            return;
        }
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        const auto& state = states[static_cast<std::size_t>(player)];
        const auto muteText = state.muted
            ? L"Unmute p" + std::to_wstring(player + 1)
            : L"Mute p" + std::to_wstring(player + 1);
        if (panelMuteButton_) {
            SetWindowTextW(panelMuteButton_, muteText.c_str());
            EnableWindow(panelMuteButton_, state.active ? TRUE : FALSE);
        }
        if (panelStopButton_)
            EnableWindow(panelStopButton_, state.active ? TRUE : FALSE);
        updatePlayerControlSliders(states);
    }

    void updatePlayerControlSliders(
        const std::array<EditorPlayerState, 8>& states) {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_)
            return;
        const auto& state = states[static_cast<std::size_t>(player)];
        const int effectSlot = selectedEffectSlotIndex();
        if (effectSlot >= 0 &&
            effectSlot < static_cast<int>(state.effectIndices.size())) {
            const auto externalNames = host_->editorExternalSynthNames();
            const int effectIndex =
                state.effectIndices[static_cast<std::size_t>(effectSlot)];
            if (effectIndex >= 0 &&
                effectIndex < static_cast<int>(externalNames.size())) {
                const auto effectName = externalNames[
                    static_cast<std::size_t>(effectIndex)];
                const auto specs =
                    dexter::editor::playerEffectControlSpecs(effectName);
                for (std::size_t index = 0;
                     index < playerControlSliders_.size(); ++index) {
                    const bool available = index < specs.size();
                    const int visibility = available ? SW_SHOW : SW_HIDE;
                    ShowWindow(playerControlLabels_[index], visibility);
                    ShowWindow(playerControlSliders_[index], visibility);
                    ShowWindow(playerControlValues_[index], visibility);
                    ShowWindow(playerControlRanges_[index], visibility);
                    if (!available)
                        continue;
                    std::string label = specs[index].name;
                    if (label.rfind("fx_", 0) == 0)
                        label.erase(0, 3);
                    const auto labelText = fromUtf8(label);
                    SetWindowTextW(playerControlLabels_[index],
                                   labelText.c_str());
                    const auto rangeText =
                        fromUtf8(effectControlRangeText(specs[index]));
                    SetWindowTextW(playerControlRanges_[index],
                                   rangeText.c_str());
                    if (playerControlSliders_[index]) {
                        SendMessageW(playerControlSliders_[index],
                                     TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
                        if (activePlayerSlider_ !=
                            static_cast<int>(index)) {
                            const float normalized = std::clamp(
                                state.effectControlValues[
                                    static_cast<std::size_t>(effectSlot)][index],
                                0.0f, 1.0f);
                            const int position = static_cast<int>(
                                std::lround(normalized * 1000.0f));
                            SendMessageW(playerControlSliders_[index],
                                         TBM_SETPOS, TRUE, position);
                            const auto valueText = fromUtf8(
                                effectControlValueText(specs[index],
                                                      normalized));
                            if (playerControlValues_[index])
                                SetWindowTextW(playerControlValues_[index],
                                               valueText.c_str());
                        }
                        EnableWindow(playerControlSliders_[index],
                                     state.active ? TRUE : FALSE);
                    }
                }
                return;
            }
        }
        if (effectSlot < 0 && selectedPanelTab() == 0 &&
            state.userSynthControlCount > 0) {
            for (std::size_t index = 0;
                 index < playerControlSliders_.size(); ++index) {
                const bool available =
                    index < state.userSynthControlCount;
                const int visibility = available ? SW_SHOW : SW_HIDE;
                ShowWindow(playerControlLabels_[index], visibility);
                ShowWindow(playerControlSliders_[index], visibility);
                ShowWindow(playerControlValues_[index], visibility);
                ShowWindow(playerControlRanges_[index], visibility);
                if (!available)
                    continue;
                const auto& control = state.userSynthControls[index];
                const auto label = fromUtf8(control.name);
                SetWindowTextW(playerControlLabels_[index], label.c_str());
                const auto range = fromUtf8(
                    userSynthControlRangeText(control));
                SetWindowTextW(playerControlRanges_[index], range.c_str());
                if (!playerControlSliders_[index])
                    continue;
                SendMessageW(playerControlSliders_[index], TBM_SETRANGE, TRUE,
                              MAKELONG(0, 1000));
                if (activePlayerSlider_ != static_cast<int>(index)) {
                    const float normalized = std::clamp(
                        control.normalizedValue, 0.0f, 1.0f);
                    SendMessageW(playerControlSliders_[index], TBM_SETPOS,
                                 TRUE, static_cast<LPARAM>(
                                     std::lround(normalized * 1000.0f)));
                    const auto value = fromUtf8(
                        userSynthControlValueText(control, normalized));
                    if (playerControlValues_[index])
                        SetWindowTextW(playerControlValues_[index],
                                       value.c_str());
                }
                EnableWindow(playerControlSliders_[index],
                             state.active ? TRUE : FALSE);
            }
            return;
        }
        const auto cutoffPosition = static_cast<int>(std::lround(
            std::clamp(std::log(std::clamp(state.cutoffHz, 20.0f, 20000.0f) /
                                    20.0f) /
                           std::log(1000.0f),
                       0.0f, 1.0f) *
            1000.0f));
        const std::array<int, 7> positions{
            static_cast<int>(std::lround(
                std::clamp(state.amplitude, 0.0f, 1.0f) * 100.0f)),
            static_cast<int>(std::lround(
                (std::clamp(state.pan, -1.0f, 1.0f) + 1.0f) * 100.0f)),
            cutoffPosition,
            static_cast<int>(std::lround(
                std::clamp(state.effectMix, 0.0f, 1.0f) * 100.0f)),
            static_cast<int>(std::lround(
                std::clamp(state.bits, 0.0f, 24.0f))),
            static_cast<int>(std::lround(
                std::clamp(state.crush, 1.0f, 128.0f))),
            static_cast<int>(std::lround(
                (std::clamp(state.pshift, -24.0f, 24.0f) + 24.0f) *
                10.0f))};
        const std::array<std::wstring, 7> valueTexts{
            fromUtf8(sliderValueText(0, positions[0])),
            fromUtf8(sliderValueText(1, positions[1])),
            fromUtf8(sliderValueText(2, positions[2])),
            fromUtf8(sliderValueText(3, positions[3])),
            fromUtf8(sliderValueText(4, positions[4])),
            fromUtf8(sliderValueText(5, positions[5])),
            fromUtf8(sliderValueText(6, positions[6]))};
        static constexpr std::array<const wchar_t*, 7> labels{
            L"Level", L"Pan", L"Cutoff", L"FX mix", L"Bits", L"Crush",
            L"PShift"};
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            ShowWindow(playerControlRanges_[index], SW_HIDE);
            if (activePlayerSlider_ == static_cast<int>(index))
                continue;
            const auto control = index == 0 ? EditorPlayerControl::Amplitude
                : index == 1 ? EditorPlayerControl::Pan
                : index == 2 ? EditorPlayerControl::Cutoff
                : index == 3 ? EditorPlayerControl::EffectMix
                : index == 4 ? EditorPlayerControl::BitDepth
                : index == 5 ? EditorPlayerControl::CrushRate
                             : EditorPlayerControl::PShift;
            const bool available = editorPlayerControlAvailable(
                control, state.effectMixAvailable,
                state.bitcrusherEnabled, state.pshiftEnabled);
            const int visibility = (control == EditorPlayerControl::EffectMix &&
                                    !available)
                ? SW_HIDE : editorPlayerControlVisible(
                control, selectedPanelTab() == 0, true,
                state.bitcrusherEnabled, state.pshiftEnabled)
                ? SW_SHOW : SW_HIDE;
            if (playerControlLabels_[index])
                SetWindowTextW(playerControlLabels_[index], labels[index]);
            ShowWindow(playerControlLabels_[index], visibility);
            ShowWindow(playerControlSliders_[index], visibility);
            ShowWindow(playerControlValues_[index], visibility);
            if (playerControlSliders_[index]) {
                const int minimum = index == 5 ? 1 : 0;
                const int maximum = index == 6 ? 480
                    : index == 2 ? 1000
                    : index == 1 ? 200
                    : index == 4 ? 24
                    : index == 5 ? 128 : 100;
                SendMessageW(playerControlSliders_[index], TBM_SETRANGE,
                             TRUE, MAKELONG(minimum, maximum));
                SendMessageW(playerControlSliders_[index], TBM_SETPOS, TRUE,
                             positions[index]);
                EnableWindow(playerControlSliders_[index],
                              state.active && available
                                 ? TRUE
                                 : FALSE);
            }
            if (playerControlValues_[index])
                SetWindowTextW(playerControlValues_[index],
                               valueTexts[index].c_str());
        }
    }

    static std::string sliderValueText(std::size_t index, int position) {
        std::ostringstream text;
        if (index == 0 || index == 3) {
            text << std::fixed << std::setprecision(2) << position / 100.0;
        } else if (index == 1) {
            text << std::fixed << std::setprecision(2)
                 << (position / 100.0 - 1.0);
        } else if (index == 4) {
            text << position << " bits";
        } else if (index == 5) {
            text << position << "x";
        } else if (index == 6) {
            text << std::fixed << std::setprecision(1)
                 << (position / 10.0 - 24.0) << " st";
        } else {
            const double cutoff = 20.0 * std::pow(1000.0, position / 1000.0);
            text << static_cast<int>(std::lround(cutoff)) << " Hz";
        }
        return text.str();
    }

    static const char* effectControlUnit(std::string_view name) {
        if (name == "fx_tone" || name == "fx_center" ||
            name == "fx_frequency" ||
            name == "fx_rate" || name == "fx_modfreq")
            return " Hz";
        if (name == "fx_delay" || name == "fx_t60")
            return " s";
        if (name == "fx_drive")
            return " dB";
        if (name == "fx_stages")
            return " stages";
        return "";
    }

    static std::string effectControlRangeText(
        const dexter::editor::PlayerEffectControlSpec& spec) {
        std::ostringstream text;
        text << std::setprecision(3) << spec.minimum << " - "
             << std::setprecision(3) << spec.maximum
             << effectControlUnit(spec.name);
        return text.str();
    }

    static std::string effectControlValueText(
        const dexter::editor::PlayerEffectControlSpec& spec,
        float normalized) {
        const float value = spec.minimum + std::clamp(normalized, 0.0f, 1.0f) *
            (spec.maximum - spec.minimum);
        std::ostringstream text;
        text << std::setprecision(4) << value << effectControlUnit(spec.name);
        return text.str();
    }

    static float userSynthControlValue(
        const EditorPlayerState::UserSynthControl& control,
        float normalized) {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        if (control.logarithmic)
            return control.minimum * std::pow(
                control.maximum / control.minimum, normalized);
        return control.minimum +
            normalized * (control.maximum - control.minimum);
    }

    static std::string userSynthControlRangeText(
        const EditorPlayerState::UserSynthControl& control) {
        std::ostringstream text;
        text << std::setprecision(4) << control.minimum << " - "
             << std::setprecision(4) << control.maximum;
        if (control.logarithmic)
            text << " log";
        return text.str();
    }

    static std::string userSynthControlValueText(
        const EditorPlayerState::UserSynthControl& control,
        float normalized) {
        std::ostringstream text;
        text << std::setprecision(5)
             << userSynthControlValue(control, normalized);
        return text.str();
    }

    static float playerControlValue(std::size_t index, int position) {
        if (index == 0 || index == 3)
            return std::clamp(position / 100.0f, 0.0f, 1.0f);
        if (index == 1)
            return std::clamp(position / 100.0f - 1.0f, -1.0f, 1.0f);
        if (index == 4)
            return std::clamp(static_cast<float>(position), 0.0f, 24.0f);
        if (index == 5)
            return std::clamp(static_cast<float>(position), 1.0f, 128.0f);
        if (index == 6)
            return std::clamp(position / 10.0f - 24.0f, -24.0f, 24.0f);
        const double cutoff = 20.0 * std::pow(1000.0, position / 1000.0);
        return static_cast<float>(std::clamp(cutoff, 20.0, 20000.0));
    }

    void onPlayerControlSlider(std::size_t index, int scrollCode) {
        const int player = selectedPlayerIndex();
        if (!host_ || player < 0 || index >= playerControlSliders_.size())
            return;
        activePlayerSlider_ = scrollCode == TB_ENDTRACK
            ? -1 : static_cast<int>(index);
        const int position = static_cast<int>(
            SendMessageW(playerControlSliders_[index], TBM_GETPOS, 0, 0));
        const int effectSlot = selectedEffectSlotIndex();
        if (effectSlot >= 0) {
            std::array<EditorPlayerState, 8> states{};
            host_->editorPlayerStates(states);
            const auto& state = states[static_cast<std::size_t>(player)];
            const auto externalNames = host_->editorExternalSynthNames();
            if (effectSlot >= static_cast<int>(state.effectIndices.size()))
                return;
            const int effectIndex =
                state.effectIndices[static_cast<std::size_t>(effectSlot)];
            if (effectIndex < 0 ||
                effectIndex >= static_cast<int>(externalNames.size()))
                return;
            const auto specs = dexter::editor::playerEffectControlSpecs(
                externalNames[static_cast<std::size_t>(effectIndex)]);
            if (index >= specs.size())
                return;
            const float normalized = std::clamp(position / 1000.0f, 0.0f, 1.0f);
            host_->setEditorPlayerEffectControl(
                player, effectSlot, static_cast<int>(index), normalized);
            if (playerControlValues_[index]) {
                const auto label = fromUtf8(
                    effectControlValueText(specs[index], normalized));
                SetWindowTextW(playerControlValues_[index], label.c_str());
            }
            if (scrollCode == TB_ENDTRACK)
                commitPlayerEffectControlToSource(
                    player, effectSlot, specs[index], normalized);
            return;
        }
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        const auto& state = states[static_cast<std::size_t>(player)];
        if (index < state.userSynthControlCount) {
            const auto& control = state.userSynthControls[index];
            const float normalized =
                std::clamp(position / 1000.0f, 0.0f, 1.0f);
            host_->setEditorPlayerUserSynthControl(
                player, static_cast<int>(index), normalized);
            if (playerControlValues_[index]) {
                const auto label = fromUtf8(
                    userSynthControlValueText(control, normalized));
                SetWindowTextW(playerControlValues_[index], label.c_str());
            }
            return;
        }
        const float value = playerControlValue(index, position);
        const auto control = index == 0 ? EditorPlayerControl::Amplitude
            : index == 1 ? EditorPlayerControl::Pan
            : index == 2 ? EditorPlayerControl::Cutoff
            : index == 3 ? EditorPlayerControl::EffectMix
            : index == 4 ? EditorPlayerControl::BitDepth
            : index == 5 ? EditorPlayerControl::CrushRate
                         : EditorPlayerControl::PShift;
        host_->setEditorPlayerControl(player, control, value);
        if (playerControlValues_[index]) {
            const auto label = fromUtf8(sliderValueText(index, position));
            SetWindowTextW(playerControlValues_[index], label.c_str());
        }
        if (scrollCode == TB_ENDTRACK)
            commitPlayerControlToSource(player, index, value);
    }

    void commitPlayerControlToSource(int player, std::size_t index,
                                     float value) {
        if (!editor_ || player < 0 || index >= playerControlSliders_.size())
            return;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentForIndex(
                source, player, assignment))
            return;
        const char* keyword = index == 0 ? "amp"
            : index == 1 ? "pan"
            : index == 2 ? "cutoff"
            : index == 3 ? "fx_mix"
            : index == 4 ? "bits"
            : index == 5 ? "crush"
                         : "pshift";
        std::ostringstream number;
        number << std::fixed
               << std::setprecision(index == 2 ? 1 : index == 6 ? 1
                                            : index >= 4 ? 0 : 3)
               << value;
        const bool changed = index == 3
            ? dexter::editor::setPlayerEffectMix(
                  source, assignment, number.str())
            : dexter::editor::setPlayerArgument(
                  source, assignment, keyword, number.str());
        if (changed)
            applyEditorTextEdit(source);
    }

    void commitPlayerEffectControlToSource(
        int player, int effectSlot,
        const dexter::editor::PlayerEffectControlSpec& spec,
        float normalized) {
        if (!editor_ || player < 0 || effectSlot < 0)
            return;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentForIndex(
                source, player, assignment))
            return;
        const float value = spec.minimum + std::clamp(
            normalized, 0.0f, 1.0f) * (spec.maximum - spec.minimum);
        std::ostringstream number;
        number << std::setprecision(7) << value;
        if (dexter::editor::setPlayerEffectControl(
                source, assignment, static_cast<std::size_t>(effectSlot),
                spec.name, number.str()))
            applyEditorTextEdit(source);
    }

    void toggleSelectedPlayerMute() {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_)
            return;
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        host_->setEditorPlayerMuted(player,
                                    !states[static_cast<std::size_t>(player)].muted);
        updatePlayerActionButtons();
    }

    void stopSelectedPlayer() {
        const int player = selectedPlayerIndex();
        if (player >= 0 && host_)
            host_->stopEditorPlayer(player);
    }

    void openSamplesFolder() {
        const auto portable = portableScriptDirectory();
        if (portable.empty()) {
            MessageBoxW(window_, L"The plugin bundle path is not available.",
                        L"Samples", MB_OK | MB_ICONERROR);
            return;
        }
        std::error_code error;
        const auto directory = portable / L"Samples";
        std::filesystem::create_directories(directory, error);
        if (error) {
            MessageBoxW(window_, L"Could not create or open Portable\\Samples.",
                        L"Samples", MB_OK | MB_ICONERROR);
            return;
        }
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(window_, L"open", directory.c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL));
        if (result <= 32)
            MessageBoxW(window_, L"Windows could not open Portable\\Samples.",
                        L"Samples", MB_OK | MB_ICONERROR);
    }

    void layoutChildren(int width, int height) {
        if (!editor_)
            return;
        const int buttonY = kMargin;
        MoveWindow(runButton_, kMargin, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(stopButton_, kMargin + 60, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(openButton_, kMargin + 120, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(saveButton_, kMargin + 180, buttonY, 62, kToolbarHeight - 4, TRUE);
        MoveWindow(saveAsButton_, kMargin + 250, buttonY, 76, kToolbarHeight - 4, TRUE);
        MoveWindow(copyErrorButton_, kMargin + 330, buttonY, 82, kToolbarHeight - 4, TRUE);
        MoveWindow(status_, kMargin + 420, buttonY, std::max(100, width - 440),
                   kToolbarHeight - 4, TRUE);
        const int gainRowY = kMargin + kToolbarHeight;
        if (masterGainSlider_) {
            MoveWindow(masterGainLabel_, kMargin, gainRowY + 2, 60, 22, TRUE);
            MoveWindow(masterGainSlider_, kMargin + 68, gainRowY,
                       210, 24, TRUE);
            MoveWindow(masterGainValue_, kMargin + 286, gainRowY + 2,
                       64, 22, TRUE);
        }
        const int languageRowY = kMargin + kToolbarHeight + kGainRowHeight;
        MoveWindow(pythonTabButton_, kMargin, languageRowY, 116, 22, TRUE);
        MoveWindow(superColliderTabButton_, kMargin + 120, languageRowY,
                   150, 22, TRUE);
        const int editorTop = languageRowY + 24;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        const int panelX = width - kMargin - kPanelWidth;
        const int editorWidth = std::max(280, panelX - kMargin - 12);
        MoveWindow(editor_, kMargin, editorTop, editorWidth, editorHeight, TRUE);
        MoveWindow(panelTabs_, panelX, editorTop, kPanelWidth, 28, TRUE);
        const int panelBottom = editorTop + editorHeight - 27;
        const int controlsHeight =
            static_cast<int>(playerControlSliders_.size()) * 34;
        const int controlsTop = panelBottom - controlsHeight - 4;
        MoveWindow(panelList_, panelX + 2, editorTop + 30, kPanelWidth - 4,
                   std::max(80, controlsTop - (editorTop + 35)), TRUE);
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            const int rowY = controlsTop + static_cast<int>(index) * 34;
            MoveWindow(playerControlLabels_[index], panelX + 4, rowY,
                       112, 16, TRUE);
            MoveWindow(playerControlRanges_[index], panelX + 118, rowY,
                       76, 16, TRUE);
            MoveWindow(playerControlValues_[index], panelX + 196, rowY,
                       66, 16, TRUE);
            MoveWindow(playerControlSliders_[index], panelX + 4, rowY + 15,
                       258, 18, TRUE);
        }
        MoveWindow(panelMuteButton_, panelX + 2, editorTop + editorHeight - 27,
                   88, 24, TRUE);
        MoveWindow(panelStopButton_, panelX + 96, editorTop + editorHeight - 27,
                   88, 24, TRUE);
        MoveWindow(panelSamplesButton_, panelX + 2,
                   editorTop + editorHeight - 27, 160, 24, TRUE);
        MoveWindow(panelVersion_, panelX + kPanelWidth - 84,
                   editorTop + editorHeight - 27, 80, 24, TRUE);
        const int footerY = height - kStatusHeight - kMargin;
        MoveWindow(fileName_, kMargin, footerY, std::max(120, width - 250),
                   kStatusHeight, TRUE);
        MoveWindow(brand_, std::max(kMargin, width - 220), footerY, 210, kStatusHeight, TRUE);
    }

    void updateSaveButton() {
        if (saveButton_)
            SetWindowTextW(saveButton_, documentDirty_ ? L"Save*" : L"Save");
        updateFileName();
    }

    void updateFileName() {
        if (!fileName_)
            return;
        std::wstring name = currentFilePath_.empty()
                                ? (activeTab_ == 0
                                       ? L"Untitled Python script"
                                       : L"Untitled SuperCollider script")
                                : std::filesystem::path(currentFilePath_).filename().wstring();
        if (documentDirty_)
            name += L"  *";
        SetWindowTextW(fileName_, name.c_str());
    }

    void updateMasterGainText(int position) {
        if (!masterGainValue_)
            return;
        const auto text = std::to_wstring(std::clamp(
            position, 0, kDexterMasterGainMaximumPercent)) + L"%";
        SetWindowTextW(masterGainValue_, text.c_str());
    }

    std::string editorUtf8Source() const {
        if (!editor_)
            return {};
        const auto length = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETTEXTLENGTH, 0, 0));
        std::string source(length + 1, '\0');
        SendMessageW(editor_, SCI_GETTEXT, source.size(),
                     reinterpret_cast<LPARAM>(source.data()));
        source.resize(length);
        return source;
    }

    std::wstring editorSource() const {
        return fromUtf8(editorUtf8Source());
    }

    void applyEditorTextEdit(const std::string& source) {
        if (!editor_)
            return;
        if (source.size() > kMaxScriptBytes) {
            SetWindowTextW(status_, L"Edit would exceed the 256 KiB script limit.");
            return;
        }
        const auto previous = editorUtf8Source();
        if (source == previous)
            return;
        std::size_t prefix = 0;
        while (prefix < source.size() && prefix < previous.size() &&
               source[prefix] == previous[prefix])
            ++prefix;
        std::size_t suffix = 0;
        while (suffix < source.size() - prefix &&
               suffix < previous.size() - prefix &&
               source[source.size() - suffix - 1] ==
                   previous[previous.size() - suffix - 1])
            ++suffix;
        const auto oldEnd = previous.size() - suffix;
        const auto newEnd = source.size() - suffix;
        const auto oldCaret = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
        const auto oldAnchor = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETANCHOR, 0, 0));
        updatingEditor_ = true;
        SendMessageW(editor_, SCI_BEGINUNDOACTION, 0, 0);
        SendMessageW(editor_, SCI_SETTARGETSTART, prefix, 0);
        SendMessageW(editor_, SCI_SETTARGETEND, oldEnd, 0);
        SendMessageW(editor_, SCI_REPLACETARGET, newEnd - prefix,
                     reinterpret_cast<LPARAM>(source.data() + prefix));
        SendMessageW(editor_, SCI_ENDUNDOACTION, 0, 0);
        const auto remap = [prefix, oldEnd, newEnd](std::size_t position) {
            if (position <= prefix)
                return position;
            if (position < oldEnd)
                return newEnd;
            return newEnd + position - oldEnd;
        };
        const auto caret = std::min(remap(oldCaret), source.size());
        const auto anchor = std::min(remap(oldAnchor), source.size());
        SendMessageW(editor_, SCI_SETSEL, anchor, caret);
        updatingEditor_ = false;
        currentSource_ = fromUtf8(source);
        sourceInitialized_ = true;
        documentDirty_ = true;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        updateSaveButton();
        if (activeTab_ == 0) {
            pythonSource_ = currentSource_;
            pythonSourceInitialized_ = true;
            pythonDirty_ = true;
            if (host_)
                host_->setEditorSource(source);
        } else {
            superColliderSource_ = currentSource_;
            superColliderSourceInitialized_ = true;
            superColliderDirty_ = true;
            if (host_)
                host_->setSuperColliderSource(source);
        }
    }

    bool showPlayerContextMenu(HWND editor, LPARAM coordinates) {
        if (!host_ || activeTab_ != 0 || !editor || editor != editor_)
            return false;
        POINT screenPoint{};
        const POINTS packed = MAKEPOINTS(coordinates);
        if (packed.x == -1 && packed.y == -1) {
            const auto caret = static_cast<int>(
                SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
            const auto caretPoint = static_cast<LPARAM>(
                SendMessageW(editor_, SCI_POINTXFROMPOSITION, 0, caret));
            const auto caretY = static_cast<LPARAM>(
                SendMessageW(editor_, SCI_POINTYFROMPOSITION, 0, caret));
            screenPoint.x = static_cast<LONG>(caretPoint);
            screenPoint.y = static_cast<LONG>(caretY);
            ClientToScreen(editor_, &screenPoint);
        } else {
            screenPoint.x = packed.x;
            screenPoint.y = packed.y;
        }
        POINT clientPoint = screenPoint;
        if (!ScreenToClient(editor_, &clientPoint))
            return false;
        const auto position = static_cast<std::size_t>(SendMessageW(
            editor_, kSciPositionFromPointClose, clientPoint.x, clientPoint.y));
        if (position > editorUtf8Source().size())
            return false;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerLineAt(source, position, assignment,
                                               true))
            return false;

        HMENU menu = CreatePopupMenu();
        HMENU instruments = CreatePopupMenu();
        HMENU effects = CreatePopupMenu();
        if (!menu || !instruments || !effects) {
            if (menu) DestroyMenu(menu);
            if (instruments) DestroyMenu(instruments);
            if (effects) DestroyMenu(effects);
            return true;
        }
        const auto externalNames = host_->editorExternalSynthNames();
        std::vector<std::string> embedded(kEmbeddedSynthNames.begin(),
                                          kEmbeddedSynthNames.end());
        const auto instrumentsCatalog =
            dexter::editor::makeInstrumentCatalog(embedded, externalNames);
        const auto effectsCatalog =
            dexter::editor::makeEffectCatalog(externalNames);
        for (std::size_t index = 0; index < instrumentsCatalog.size(); ++index)
            AppendMenuW(
                instruments, MF_STRING |
                    (instrumentsCatalog[index].available ? 0 : MF_GRAYED),
                static_cast<UINT_PTR>(kContextCatalogSynthBaseId + index),
                fromUtf8(instrumentsCatalog[index].label).c_str());
        for (std::size_t index = 0; index < effectsCatalog.size(); ++index)
            AppendMenuW(
                effects, MF_STRING |
                    (effectsCatalog[index].available ? 0 : MF_GRAYED),
                static_cast<UINT_PTR>(kContextCatalogEffectBaseId + index),
                fromUtf8(effectsCatalog[index].label).c_str());
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(instruments),
                    L"Change instrument");
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(effects),
                    L"Add effect or control");
        const auto command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x,
            screenPoint.y, 0, window_, nullptr);
        DestroyMenu(menu);
        if (command >= kContextCatalogSynthBaseId &&
            command < kContextCatalogSynthBaseId +
                          static_cast<int>(instrumentsCatalog.size())) {
            source = editorUtf8Source();
            std::string failure;
            if (dexter::editor::applyCatalogEntry(
                    source, position,
                    instrumentsCatalog[command - kContextCatalogSynthBaseId],
                    failure)) {
                applyEditorTextEdit(source);
                SetWindowTextW(status_, L"Instrument inserted. Click Run to apply it.");
            } else {
                SetWindowTextW(status_, fromUtf8(failure).c_str());
            }
        } else if (command >= kContextCatalogEffectBaseId &&
                   command < kContextCatalogEffectBaseId +
                                 static_cast<int>(effectsCatalog.size())) {
            source = editorUtf8Source();
            std::string failure;
            if (dexter::editor::applyCatalogEntry(
                    source, position,
                    effectsCatalog[command - kContextCatalogEffectBaseId],
                    failure)) {
                applyEditorTextEdit(source);
                SetWindowTextW(status_, L"Added to script. Click Run to apply it.");
            } else {
                SetWindowTextW(status_, fromUtf8(failure).c_str());
            }
        }
        return true;
    }

    void setEditorSource(const std::wstring& source) {
        if (!editor_)
            return;
        updatingEditor_ = true;
        setEditorWindowText(editor_, source);
        updatingEditor_ = false;
        currentSource_ = source;
        sourceInitialized_ = true;
        documentDirty_ = false;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        if (activeTab_ == 0) {
            pythonSource_ = source;
            pythonSourceInitialized_ = true;
            pythonDirty_ = false;
            if (host_)
                host_->setEditorSource(toUtf8(source));
        } else {
            superColliderSource_ = source;
            superColliderSourceInitialized_ = true;
            superColliderDirty_ = false;
            if (host_)
                host_->setSuperColliderSource(toUtf8(source));
        }
        updateSaveButton();
    }

    bool confirmReplaceDocument() {
        if (!documentDirty_)
            return true;
        const int answer = MessageBoxW(
            window_, L"Save changes before replacing this script?",
            L"Unsaved script changes", MB_YESNOCANCEL | MB_ICONWARNING);
        if (answer == IDYES)
            return saveSource();
        return answer == IDNO;
    }

    bool chooseScriptPath(bool saveDialog, std::wstring& selectedPath) {
        const auto portableDirectory = portableScriptDirectory();
        if (portableDirectory.empty()) {
            MessageBoxW(window_,
                        L"Could not locate the VST3 folder for the Portable scripts directory.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(portableDirectory, directoryError);
        if (directoryError) {
            std::wstring message = L"Could not create the Portable folder:\n";
            message += portableDirectory.wstring();
            MessageBoxW(window_, message.c_str(), L"Portable scripts",
                        MB_OK | MB_ICONERROR);
            return false;
        }

        struct ComApartment {
            HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            bool uninitialize = SUCCEEDED(result);
            ~ComApartment() { if (uninitialize) CoUninitialize(); }
        } apartment;
        if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) {
            MessageBoxW(window_, L"Could not initialize Windows file dialogs.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        IFileDialog* dialog = nullptr;
        const CLSID dialogClass = saveDialog ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
        HRESULT result = CoCreateInstance(dialogClass, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&dialog));
        if (FAILED(result) || !dialog) {
            MessageBoxW(window_, L"Could not create a Windows file dialog.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }
        struct ComRelease {
            IUnknown* object;
            ~ComRelease() { if (object) object->Release(); }
        } dialogRelease{dialog};

        static constexpr COMDLG_FILTERSPEC pythonFilters[] = {
            {L"FoxDot/Python scripts (*.py)", L"*.py"},
            {L"All files (*.*)", L"*.*"}};
        static constexpr COMDLG_FILTERSPEC superColliderFilters[] = {
            {L"SuperCollider scripts (*.scd; *.sc)", L"*.scd;*.sc"},
            {L"All files (*.*)", L"*.*"}};
        const auto* filters =
            activeTab_ == 0 ? pythonFilters : superColliderFilters;
        result = dialog->SetFileTypes(2, filters);
        if (SUCCEEDED(result))
            result = dialog->SetFileTypeIndex(1);
        if (SUCCEEDED(result))
            result = dialog->SetDefaultExtension(
                activeTab_ == 0 ? L"py" : L"scd");

        IShellItem* folder = nullptr;
        if (SUCCEEDED(result))
            result = SHCreateItemFromParsingName(portableDirectory.c_str(), nullptr,
                                                 IID_PPV_ARGS(&folder));
        ComRelease folderRelease{folder};
        if (SUCCEEDED(result))
            result = dialog->SetFolder(folder);

        DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        options |= saveDialog ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST;
        if (SUCCEEDED(result))
            result = dialog->SetOptions(options);
        if (SUCCEEDED(result) && saveDialog)
            result = dialog->SetFileName(
                activeTab_ == 0 ? L"new-pattern.py" : L"new-score.scd");
        if (FAILED(result)) {
            std::wstring message = L"Could not set the dialog folder to:\n";
            message += portableDirectory.wstring();
            MessageBoxW(window_, message.c_str(), L"Portable scripts",
                        MB_OK | MB_ICONERROR);
            return false;
        }

        result = dialog->Show(window_);
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            return false;
        if (FAILED(result)) {
            MessageBoxW(window_, L"The file dialog could not be displayed.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        IShellItem* selectedItem = nullptr;
        result = dialog->GetResult(&selectedItem);
        ComRelease selectedRelease{selectedItem};
        if (FAILED(result) || !selectedItem)
            return false;

        PWSTR selectedFilePath = nullptr;
        result = selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedFilePath);
        if (FAILED(result) || !selectedFilePath)
            return false;
        selectedPath = selectedFilePath;
        CoTaskMemFree(selectedFilePath);
        return true;
    }

    void openSource() {
        std::wstring selectedPath;
        if (!chooseScriptPath(false, selectedPath) || !confirmReplaceDocument())
            return;

        const std::filesystem::path path(selectedPath);
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > kMaxScriptBytes) {
            MessageBoxW(window_,
                        error ? L"Could not read that script file."
                              : L"Script files must be 256 KB or smaller.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            MessageBoxW(window_, L"Could not open that script file.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        std::string source(static_cast<std::size_t>(size), '\0');
        if (!source.empty())
            input.read(source.data(), static_cast<std::streamsize>(source.size()));
        if ((!input && !source.empty()) || source.find('\0') != std::string::npos) {
            MessageBoxW(window_, L"That file could not be read as a text script.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        if (source.size() >= 3 &&
            static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF) {
            source.erase(0, 3);
        }
        const auto wideSource = fromUtf8(source);
        if (!source.empty() && wideSource.empty()) {
            MessageBoxW(window_, L"Scripts must use valid UTF-8 text.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        if (wideSource.size() > kMaxScriptCharacters) {
            MessageBoxW(window_, L"Script files must contain 65,536 characters or fewer.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }

        setEditorSource(wideSource);
        currentFilePath_ = selectedPath;
        persistActiveDocument();
        updateFileName();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script loaded. Press Run for compatibility check and evaluation."
            : L"SuperCollider script loaded. Press Run to send it to the embedded engine.");
    }

    bool saveSourceToPath(const std::wstring& path) {
        const auto source = toUtf8(editorSource());
        if (source.size() > kMaxScriptBytes) {
            MessageBoxW(window_, L"Scripts must be 256 KiB or smaller.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        std::ofstream output(std::filesystem::path(path),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            MessageBoxW(window_, L"Could not write that script file.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        output.write(source.data(), static_cast<std::streamsize>(source.size()));
        output.flush();
        if (!output) {
            MessageBoxW(window_, L"An error occurred while saving the script.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool saveSourceAs() {
        std::wstring selectedPath;
        if (!chooseScriptPath(true, selectedPath))
            return false;
        if (!saveSourceToPath(selectedPath))
            return false;
        currentFilePath_ = std::move(selectedPath);
        updateFileName();
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        documentDirty_ = false;
        persistActiveDocument();
        updateSaveButton();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script saved." : L"SuperCollider script saved.");
        return true;
    }

    bool saveSource() {
        if (currentFilePath_.empty())
            return saveSourceAs();
        if (!saveSourceToPath(currentFilePath_))
            return false;
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        documentDirty_ = false;
        persistActiveDocument();
        updateSaveButton();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script saved." : L"SuperCollider script saved.");
        return true;
    }

    void runSource() {
        if (!editor_ || !host_)
            return;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        const auto source = toUtf8(editorSource());
        if (source.size() > kMaxScriptBytes) {
            SetWindowTextW(status_, L"Script exceeds the 256 KiB size limit.");
            return;
        }
        if (host_->scriptWorkerBusy()) {
            SetWindowTextW(
                status_,
                L"The previous script is still processing. Wait for Run to be ready before submitting another.");
            return;
        }
        if (activeTab_ == 1) {
            if (!superColliderExecutionWarningShown_) {
                const int answer = MessageBoxW(
                    window_,
                    L"This tab runs full SuperCollider code through bundled sclang. "
                    L"It can read or write files, start processes, and make network "
                    L"connections. Run only code you trust. Continue?",
                    L"Unrestricted SuperCollider code",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (answer != IDYES) {
                    SetWindowTextW(status_, L"SuperCollider code was not run.");
                    return;
                }
                superColliderExecutionWarningShown_ = true;
            }
            host_->setSuperColliderSource(source);
            host_->submitSuperColliderCode(source);
            SetWindowTextW(status_,
                L"Queued for headless sclang; it uses this VST's embedded engine.");
            return;
        }
        const auto report = host_->checkCompatibility(source);
        if (report.rfind("No unsupported constructs detected", 0) != 0) {
            if (report.rfind(
                    "The Python script worker is still processing the previous Run.",
                    0) == 0) {
                SetWindowTextW(status_, fromUtf8(report).c_str());
                return;
            }
            const auto message = fromUtf8(report);
            lastDiagnostic_ = message.empty()
                                  ? L"Compatibility check failed; script was not run."
                                  : message;
            copyDiagnosticPinned_ = true;
            if (copyErrorButton_)
                EnableWindow(copyErrorButton_, TRUE);
            SetWindowTextW(status_, L"Compatibility items found; script was not run. Click Copy Error for details.");
            return;
        }
        host_->submitCode(source);
        updateRunButtonState();
        SetWindowTextW(status_, L"Compatibility check passed; evaluating Python...");
    }

    void copyLastError() {
        if (lastDiagnostic_.empty()) {
            SetWindowTextW(status_, L"There is no error or compatibility report to copy yet.");
            return;
        }
        if (!OpenClipboard(window_)) {
            SetWindowTextW(status_, L"Clipboard is busy. Try Copy Error again.");
            return;
        }
        EmptyClipboard();
        const SIZE_T bytes = (lastDiagnostic_.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) {
            CloseClipboard();
            SetWindowTextW(status_, L"Could not allocate clipboard text.");
            return;
        }
        auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
        if (!destination) {
            GlobalFree(memory);
            CloseClipboard();
            SetWindowTextW(status_, L"Could not lock clipboard text.");
            return;
        }
        std::memcpy(destination, lastDiagnostic_.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) {
            GlobalFree(memory);
            CloseClipboard();
            SetWindowTextW(status_, L"Windows could not place the report on the clipboard.");
            return;
        }
        CloseClipboard();
        copyDiagnosticPinned_ = true;
        SetWindowTextW(status_, L"Full error/report copied to the clipboard.");
    }

    void refreshStatus() {
        if (!host_ || !status_)
            return;
        updateRunButtonState();
        const int gainPosition = static_cast<int>(
            std::lround(std::clamp(host_->masterOutputGain(), 0.0f,
                                   kDexterMasterGainMaximum) * 100.0f));
        if (masterGainSlider_)
            SendMessageW(masterGainSlider_, TBM_SETPOS, TRUE, gainPosition);
        updateMasterGainText(gainPosition);
        const auto restoredSource = activeTab_ == 0
            ? fromUtf8(host_->editorSource())
            : fromUtf8(host_->superColliderSource());
        const bool hasRestoredSource = activeTab_ == 0
            ? host_->hasEditorSource() : host_->hasSuperColliderSource();
        if (!documentDirty_ && hasRestoredSource &&
            restoredSource != currentSource_) {
            currentFilePath_.clear();
            setEditorSource(restoredSource);
        }
        if (copyDiagnosticPinned_)
            return;
        const auto statusText = activeTab_ == 0
            ? host_->editorStatus() : host_->superColliderStatus();
        const auto message = fromUtf8(statusText);
        const bool hasDiagnostic =
            message.find(L"Traceback") != std::wstring::npos ||
            message.find(L"Error") != std::wstring::npos ||
            message.find(L"error") != std::wstring::npos ||
            message.find(L"exception") != std::wstring::npos ||
            message.find(L"invalid") != std::wstring::npos ||
            message.find(L"malformed") != std::wstring::npos ||
            message.find(L"failed") != std::wstring::npos ||
            message.find(L"unsupported") != std::wstring::npos ||
            message.find(L"overlap") != std::wstring::npos ||
            message.find(L"exceeds") != std::wstring::npos ||
            message.find(L"Could not") != std::wstring::npos ||
            message.find(L"could not") != std::wstring::npos ||
            message.find(L"timed out") != std::wstring::npos ||
            message.find(L"Timed out") != std::wstring::npos;
        if (hasDiagnostic) {
            lastDiagnostic_ = message;
            copyDiagnosticPinned_ = true;
        } else {
            lastDiagnostic_.clear();
        }
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, hasDiagnostic ? TRUE : FALSE);
        const auto newline = message.find_first_of(L"\r\n");
        const auto visibleMessage = newline == std::wstring::npos
            ? message : message.substr(0, newline);
        SetWindowTextW(status_, visibleMessage.c_str());
    }

    void updateRunButtonState() {
        if (!host_ || !runButton_)
            return;
        const bool busy = host_->scriptWorkerBusy();
        EnableWindow(runButton_, busy ? FALSE : TRUE);
        SetWindowTextW(runButton_, busy ? L"Busy" : L"Run");
    }

    ICodeEditorHost* host_ = nullptr;
    Steinberg::FUnknown* keepAlive_ = nullptr;
    bool scintillaClassesAcquired_ = false;
    Steinberg::IPlugFrame* frame_ = nullptr;
    HWND window_ = nullptr;
    HWND editor_ = nullptr;
    HWND pythonEditor_ = nullptr;
    HWND superColliderEditor_ = nullptr;
    HWND pythonTabButton_ = nullptr;
    HWND superColliderTabButton_ = nullptr;
    int activeTab_ = 0;
    HWND runButton_ = nullptr;
    HWND stopButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND saveAsButton_ = nullptr;
    HWND copyErrorButton_ = nullptr;
    HWND status_ = nullptr;
    HWND brand_ = nullptr;
    HWND fileName_ = nullptr;
    HWND masterGainSlider_ = nullptr;
    HWND masterGainValue_ = nullptr;
    HWND masterGainLabel_ = nullptr;
    HWND panelTabs_ = nullptr;
    HWND panelList_ = nullptr;
    HWND panelMuteButton_ = nullptr;
    HWND panelStopButton_ = nullptr;
    HWND panelSamplesButton_ = nullptr;
    HWND panelVersion_ = nullptr;
    std::array<HWND, 7> playerControlLabels_{};
    std::array<HWND, 7> playerControlSliders_{};
    std::array<HWND, 7> playerControlRanges_{};
    int activePlayerSlider_ = -1;
    std::array<HWND, 7> playerControlValues_{};
    std::array<EditorPlayerState, 8> lastPanelPlayers_{};
    std::vector<dexter::editor::CatalogEntry> panelCatalog_;
    std::unordered_map<std::string, bool> collapsedCatalogCategories_;
    std::unordered_map<std::string, bool> collapsedCatalogSubcategories_;
    std::unordered_map<int, std::string> catalogCategoryHeaderRows_;
    std::unordered_map<int, std::pair<std::string, std::string>>
        catalogSubcategoryHeaderRows_;
    std::array<bool, 8> expandedEffectChains_{};
    bool panelPlayersInitialized_ = false;
    bool panelEffectsInitialized_ = false;
    bool panelSynthsInitialized_ = false;
    bool activeEffectSummaryExpanded_ = false;
    int renderedPanelTab_ = -1;
    std::vector<std::string> lastPanelSynthNames_;
    dexter::editor::CatalogEntry dragCatalogEntry_{};
    POINT catalogDragOrigin_{};
    bool dragCatalogPending_ = false;
    bool catalogDragActive_ = false;
    std::wstring lastDiagnostic_;
    bool copyDiagnosticPinned_ = false;
    std::wstring currentFilePath_;
    std::wstring currentSource_;
    std::wstring pythonFilePath_;
    std::wstring superColliderFilePath_;
    std::wstring pythonSource_;
    std::wstring superColliderSource_;
    bool updatingEditor_ = false;
    bool documentDirty_ = false;
    bool sourceInitialized_ = false;
    bool pythonDirty_ = false;
    bool superColliderDirty_ = false;
    bool pythonSourceInitialized_ = false;
    bool superColliderSourceInitialized_ = false;
    bool superColliderExecutionWarningShown_ = false;
};
} // namespace

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost* host,
                                          Steinberg::FUnknown* keepAlive) {
    return new CodeEditorView(host, keepAlive);
}

#else

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost*, Steinberg::FUnknown*) {
    return nullptr;
}

#endif
