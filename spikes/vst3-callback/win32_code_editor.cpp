#include "win32_code_editor.h"
#include "lifecycle_trace.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "Scintilla.h"
#include "ILexer.h"
#include "Lexilla.h"
#include "SciLexer.h"


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
constexpr int kViewWidth = 720;
constexpr int kViewHeight = 480;
constexpr int kMargin = 10;
constexpr int kToolbarHeight = 32;
constexpr int kStatusHeight = 24;
constexpr std::size_t kMaxScriptBytes = 256 * 1024;
constexpr std::size_t kMaxScriptCharacters = 65536;
constexpr char kPythonKeywords[] =
    "and as assert async await break class continue def del elif else except finally "
    "for from global if import in is lambda nonlocal not or pass raise return try "
    "while with yield False None True";
constexpr char kFoxDotApiKeywords[] =
    "Clock Scale Root Player P PGroup var linvar play stop sine triangle saw pulse bass varsaw dirt jbass noise "
    "dub growl soprano crunch lazer rave bell gong marimba pluck twang donk squish "
    "charm spark blip scatter orient zap fuzz bug ripple "
    "viola nylon arpy dab sawbass dbass prophet pads pasha ambi sinepad keys "
    "klank feel glass soft quin space scratch creep snick swell razz sitar star karp "
    "vib depth lforate lfowidth rq fmod blur sample "
    "p1 p2 p3 p4 p5 p6 p7 p8 amp dur sus oct pan strum cutoff resonance room reverb "
    "echo delay mute unmute rate";
constexpr char kFoxDotCompletions[] =
    "Clock P PGroup Player Root Scale amp cutoff delay dur echo linvar mute oct pan "
    "play p1 p2 p3 p4 p5 p6 p7 p8 pulse reverb resonance room saw sine stop strum "
    "sus triangle unmute var bass varsaw dirt jbass noise dub growl soprano crunch lazer rave "
    "bell gong marimba pluck twang donk squish charm spark blip scatter orient zap fuzz bug ripple rate "
    "viola nylon arpy dab sawbass dbass prophet pads pasha ambi sinepad keys "
    "klank feel glass soft quin space scratch creep snick swell razz sitar star karp "
    "vib depth lforate lfowidth rq fmod blur sample";

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

void releaseScintillaClasses() {
    std::lock_guard<std::mutex> lock(gScintillaMutex);
    if (gScintillaViewCount == 0)
        return;
    if (--gScintillaViewCount == 0)
        Scintilla_ReleaseResources();
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
            if (!acquireScintillaClasses(moduleInstance()))
                return Steinberg::kResultFalse;
            scintillaClassesAcquired_ = true;
        }
        if (!registerWindowClass()) {
            releaseScintillaClasses();
            scintillaClassesAcquired_ = false;
            return Steinberg::kResultFalse;
        }

        window_ = CreateWindowExW(0, kWindowClass, L"Dexter U.S FoxDot",
                                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                  0, 0, kViewWidth, kViewHeight,
                                  static_cast<HWND>(parent), nullptr,
                                  moduleInstance(), this);
        if (!window_) {
            releaseScintillaClasses();
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
                if (host_)
                    host_->setEditorSource(toUtf8(currentSource_));
            }
            DestroyWindow(window_);
            window_ = nullptr;
            editor_ = runButton_ = stopButton_ = openButton_ = saveButton_ = saveAsButton_ =
                status_ = brand_ = fileName_ = copyErrorButton_ = nullptr;
        }
        if (scintillaClassesAcquired_) {
            releaseScintillaClasses();
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
        static bool registered = false;
        if (registered)
            return true;
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.hInstance = moduleInstance();
        windowClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        registered = true;
        return true;
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
            view->createChildren();
            return 0;
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
            if (header && header->hwndFrom == view->editor_) {
                const auto* notification = reinterpret_cast<const SCNotification*>(lParam);
                if (notification->nmhdr.code == SCN_MODIFIED &&
                    (notification->modificationType &
                     (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0)
                    view->onEditorChanged();
                else if (notification->nmhdr.code == SCN_CHARADDED)
                    view->onEditorCharacterAdded(notification->ch);
                return 0;
            }
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kRunId) {
                view->runSource();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kStopId) {
                if (view->host_)
                    view->host_->stopPattern();
                SetWindowTextW(view->status_, L"Stopped.");
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
            break;
        case WM_TIMER:
            view->refreshStatus();
            return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            if (view->editor_) {
                view->currentSource_ = view->editorSource();
                view->sourceInitialized_ = true;
                if (view->host_)
                    view->host_->setEditorSource(toUtf8(view->currentSource_));
            }
            view->window_ = nullptr;
            view->editor_ = view->runButton_ = view->stopButton_ =
                view->openButton_ = view->saveButton_ = view->saveAsButton_ =
                    view->status_ = view->brand_ = view->fileName_ = view->copyErrorButton_ = nullptr;
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void createChildren() {
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
        brand_ = CreateWindowExW(0, L"STATIC", L"Dexter U.S FoxDot  |  v0.4.55",
                                 WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                                 0, 0, 210, kStatusHeight, window_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBrandId)),
                                 moduleInstance(), nullptr);
        fileName_ = CreateWindowExW(0, L"STATIC", L"Untitled script",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS,
                                    0, 0, 300, kStatusHeight, window_,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFileNameId)),
                                    moduleInstance(), nullptr);
        if (!sourceInitialized_) {
            currentSource_ = host_ && host_->hasEditorSource()
                                 ? fromUtf8(host_->editorSource())
                                 : kInitialCode;
            if (!host_ && currentSource_.empty())
                currentSource_ = kInitialCode;
            sourceInitialized_ = true;
        }
        updatingEditor_ = true;
        editor_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Scintilla", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL,
                                  0, 0, 600, 380, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorId)),
                                  moduleInstance(), nullptr);
        if (editor_) {
            configureEditor();
            setEditorSource(currentSource_);
        }
        updatingEditor_ = false;
        SetTimer(window_, 1, 250, nullptr);
        layoutChildren(kViewWidth, kViewHeight);
        updateSaveButton();
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

    void onEditorChanged() {
        if (updatingEditor_)
            return;
        documentDirty_ = true;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        updateSaveButton();
        if (host_)
            host_->setEditorSource(toUtf8(editorSource()));
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
        const int editorTop = kMargin + kToolbarHeight;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        MoveWindow(editor_, kMargin, editorTop, std::max(160, width - 2 * kMargin), editorHeight, TRUE);
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
                                ? L"Untitled script"
                                : std::filesystem::path(currentFilePath_).filename().wstring();
        if (documentDirty_)
            name += L"  *";
        SetWindowTextW(fileName_, name.c_str());
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

    void setEditorSource(const std::wstring& source) {
        if (!editor_)
            return;
        updatingEditor_ = true;
        const auto utf8 = toUtf8(source);
        SendMessageW(editor_, SCI_SETTEXT, 0,
                     reinterpret_cast<LPARAM>(utf8.c_str()));
        updatingEditor_ = false;
        currentSource_ = source;
        sourceInitialized_ = true;
        documentDirty_ = false;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        if (host_)
            host_->setEditorSource(toUtf8(source));
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

        static constexpr COMDLG_FILTERSPEC filters[] = {
            {L"Python scripts (*.py)", L"*.py"},
            {L"All files (*.*)", L"*.*"}};
        result = dialog->SetFileTypes(2, filters);
        if (SUCCEEDED(result))
            result = dialog->SetFileTypeIndex(1);
        if (SUCCEEDED(result))
            result = dialog->SetDefaultExtension(L"py");

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
            result = dialog->SetFileName(L"new-pattern.py");
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
        updateFileName();
        SetWindowTextW(status_, L"Script loaded. Press Run to evaluate.");
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
        updateSaveButton();
        SetWindowTextW(status_, L"Python script saved.");
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
        updateSaveButton();
        SetWindowTextW(status_, L"Python script saved.");
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
        const auto report = host_->checkCompatibility(source);
        if (report.rfind("No unsupported constructs detected", 0) != 0) {
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
        const auto restoredSource = fromUtf8(host_->editorSource());
        if (!documentDirty_ && host_->hasEditorSource() &&
            restoredSource != currentSource_) {
            currentFilePath_.clear();
            setEditorSource(restoredSource);
        }
        if (copyDiagnosticPinned_)
            return;
        const auto message = fromUtf8(host_->editorStatus());
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
            message.find(L"could not") != std::wstring::npos;
        if (hasDiagnostic)
            lastDiagnostic_ = message;
        else
            lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, hasDiagnostic ? TRUE : FALSE);
        SetWindowTextW(status_, message.c_str());
    }

    ICodeEditorHost* host_ = nullptr;
    Steinberg::FUnknown* keepAlive_ = nullptr;
    bool scintillaClassesAcquired_ = false;
    Steinberg::IPlugFrame* frame_ = nullptr;
    HWND window_ = nullptr;
    HWND editor_ = nullptr;
    HWND runButton_ = nullptr;
    HWND stopButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND saveAsButton_ = nullptr;
    HWND copyErrorButton_ = nullptr;
    HWND status_ = nullptr;
    HWND brand_ = nullptr;
    HWND fileName_ = nullptr;
    std::wstring lastDiagnostic_;
    bool copyDiagnosticPinned_ = false;
    std::wstring currentFilePath_;
    std::wstring currentSource_;
    bool updatingEditor_ = false;
    bool documentDirty_ = false;
    bool sourceInitialized_ = false;
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
