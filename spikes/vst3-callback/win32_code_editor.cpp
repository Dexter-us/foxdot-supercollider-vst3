#include "win32_code_editor.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace {
constexpr wchar_t kWindowClass[] = L"DexterSCCodeEditorView";
constexpr int kEditorId = 101;
constexpr int kRunId = 102;
constexpr int kStopId = 103;
constexpr int kStatusId = 104;
constexpr int kExamplesId = 105;
constexpr int kOpenId = 106;
constexpr int kSaveId = 107;
constexpr int kViewWidth = 720;
constexpr int kViewHeight = 480;
constexpr int kMargin = 10;
constexpr int kToolbarHeight = 32;
constexpr int kStatusHeight = 24;
constexpr std::size_t kMaxScriptBytes = 256 * 1024;
constexpr std::size_t kMaxScriptCharacters = 65536;

struct CodeExample {
    const wchar_t* name;
    const wchar_t* source;
};

constexpr std::array<CodeExample, 4> kCodeExamples{{
    {L"Four-note pulse",
     L"tempo(120)\r\n"
     L"play([60, 64, 67, 72], step=0.5, duration=0.4)\r\n"},
    {L"Resting rhythm",
     L"tempo(110)\r\n"
     L"play([60, None, 67, None, 72, 67, None, 64], "
     L"step=0.25, duration=0.2)\r\n"},
    {L"Slow bass",
     L"tempo(88)\r\n"
     L"play([36, None, 43, None, 36, 48, 43, None], "
     L"step=0.5, duration=0.4, velocity=90)\r\n"},
    {L"Chord steps",
     L"tempo(100)\r\n"
     L"play([[60, 64, 67], [57, 60, 64], [53, 57, 60], [55, 59, 62]], "
     L"step=1, duration=0.9)\r\n"},
}};

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
        if (!parent || !type || std::strcmp(type, Steinberg::kPlatformTypeHWND) != 0)
            return Steinberg::kResultFalse;
        if (window_)
            return Steinberg::kResultOk;

        if (!registerWindowClass())
            return Steinberg::kResultFalse;

        window_ = CreateWindowExW(0, kWindowClass, L"Dexter U.S. SuperCollider Live Code",
                                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                  0, 0, kViewWidth, kViewHeight,
                                  static_cast<HWND>(parent), nullptr,
                                  moduleInstance(), this);
        return window_ ? Steinberg::kResultOk : Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API removed() override {
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
            editor_ = runButton_ = stopButton_ = openButton_ = saveButton_ =
                examples_ = status_ = nullptr;
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
        case WM_SIZE:
            view->layoutChildren(LOWORD(lParam), HIWORD(lParam));
            return 0;
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
            if (LOWORD(wParam) == kEditorId && HIWORD(wParam) == EN_CHANGE) {
                if (!view->updatingEditor_) {
                    view->documentDirty_ = true;
                    view->updateSaveButton();
                    if (view->host_)
                        view->host_->setEditorSource(toUtf8(view->editorSource()));
                }
                return 0;
            }
            if (LOWORD(wParam) == kExamplesId && HIWORD(wParam) == CBN_SELCHANGE) {
                view->loadSelectedExample();
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
                view->openButton_ = view->saveButton_ = view->examples_ =
                    view->status_ = nullptr;
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
        examples_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                        CBS_DROPDOWNLIST | WS_VSCROLL,
                                    0, 0, 160, 180, window_,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kExamplesId)),
                                    moduleInstance(), nullptr);
        if (examples_) {
            SendMessageW(examples_, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(L"Examples..."));
            for (const auto& example : kCodeExamples) {
                SendMessageW(examples_, CB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(example.name));
            }
            SendMessageW(examples_, CB_SETCURSEL, 0, 0);
        }
        status_ = CreateWindowExW(0, L"STATIC", L"Ready. Choose an example or edit code.",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  0, 0, 300, 24, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)),
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
        editor_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", currentSource_.c_str(),
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
                                      ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
                                  0, 0, 600, 380, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorId)),
                                  moduleInstance(), nullptr);
        updatingEditor_ = false;
        if (editor_) {
            SendMessageW(editor_, EM_SETLIMITTEXT, kMaxScriptCharacters, 0);
            SendMessageW(editor_, WM_SETFONT,
                         reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
        }
        SetTimer(window_, 1, 250, nullptr);
        layoutChildren(kViewWidth, kViewHeight);
        updateSaveButton();
    }

    void layoutChildren(int width, int height) {
        if (!editor_)
            return;
        const int buttonY = kMargin;
        MoveWindow(runButton_, kMargin, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(stopButton_, kMargin + 60, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(openButton_, kMargin + 120, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(saveButton_, kMargin + 180, buttonY, 62, kToolbarHeight - 4, TRUE);
        MoveWindow(examples_, kMargin + 248, buttonY, 154, kToolbarHeight - 4, TRUE);
        MoveWindow(status_, kMargin + 410, buttonY, std::max(100, width - 420),
                   kToolbarHeight - 4, TRUE);
        const int editorTop = kMargin + kToolbarHeight;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        MoveWindow(editor_, kMargin, editorTop, std::max(160, width - 2 * kMargin), editorHeight, TRUE);
    }

    void updateSaveButton() {
        if (saveButton_)
            SetWindowTextW(saveButton_, documentDirty_ ? L"Save*" : L"Save");
    }

    std::wstring editorSource() const {
        if (!editor_)
            return {};
        const int length = GetWindowTextLengthW(editor_);
        std::wstring source(static_cast<std::size_t>(length) + 1, L'\0');
        if (length > 0)
            GetWindowTextW(editor_, source.data(), length + 1);
        source.resize(static_cast<std::size_t>(length));
        return source;
    }

    void setEditorSource(const std::wstring& source) {
        if (!editor_)
            return;
        updatingEditor_ = true;
        SetWindowTextW(editor_, source.c_str());
        updatingEditor_ = false;
        currentSource_ = source;
        sourceInitialized_ = true;
        documentDirty_ = false;
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
        std::array<wchar_t, 32768> path{};
        if (saveDialog && !currentFilePath_.empty()) {
            const auto count = std::min(currentFilePath_.size(), path.size() - 1);
            std::copy_n(currentFilePath_.data(), count, path.data());
            path[count] = L'\0';
        }

        const auto portableDirectory = portableScriptDirectory();
        if (!portableDirectory.empty()) {
            std::error_code directoryError;
            std::filesystem::create_directories(portableDirectory, directoryError);
            if (directoryError) {
                MessageBoxW(window_,
                            L"Could not create the Portable folder beside the VST3. "
                            L"The dialog will still open so you can choose another folder.",
                            L"Portable scripts", MB_OK | MB_ICONWARNING);
            }
        } else {
            MessageBoxW(window_,
                        L"Could not locate the VST3 folder. The dialog will open at its default location.",
                        L"Portable scripts", MB_OK | MB_ICONWARNING);
        }

        static constexpr wchar_t filter[] =
            L"Python scripts (*.py)\0*.py\0All files (*.*)\0*.*\0";
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window_;
        dialog.lpstrInitialDir = portableDirectory.empty()
                                     ? nullptr
                                     : portableDirectory.c_str();
        dialog.lpstrFilter = filter;
        dialog.nFilterIndex = 1;
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrDefExt = L"py";
        dialog.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST |
                       OFN_NOCHANGEDIR |
                       (saveDialog ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

        const BOOL selected = saveDialog ? GetSaveFileNameW(&dialog)
                                         : GetOpenFileNameW(&dialog);
        if (!selected)
            return false;
        selectedPath = path.data();
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
        if (examples_)
            SendMessageW(examples_, CB_SETCURSEL, 0, 0);
        SetWindowTextW(status_, L"Script loaded. Press Run to evaluate.");
    }

    bool saveSourceToPath(const std::wstring& path) {
        const auto source = toUtf8(editorSource());
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

    void loadSelectedExample() {
        if (!examples_)
            return;
        const auto selection = static_cast<int>(
            SendMessageW(examples_, CB_GETCURSEL, 0, 0));
        if (selection <= 0 ||
            static_cast<std::size_t>(selection - 1) >= kCodeExamples.size()) {
            return;
        }
        const auto example = kCodeExamples[static_cast<std::size_t>(selection - 1)];
        if (confirmReplaceDocument()) {
            currentFilePath_.clear();
            setEditorSource(example.source);
            SetWindowTextW(status_, L"Example loaded. Press Run to evaluate.");
        }
        SendMessageW(examples_, CB_SETCURSEL, 0, 0);
    }

    void runSource() {
        if (!editor_ || !host_)
            return;
        host_->submitCode(toUtf8(editorSource()));
        SetWindowTextW(status_, L"Evaluating Python...");
    }

    void refreshStatus() {
        if (!host_ || !status_)
            return;
        const auto restoredSource = fromUtf8(host_->editorSource());
        if (!documentDirty_ && host_->hasEditorSource() &&
            restoredSource != currentSource_) {
            setEditorSource(restoredSource);
        }
        const auto message = fromUtf8(host_->editorStatus());
        SetWindowTextW(status_, message.c_str());
    }

    ICodeEditorHost* host_ = nullptr;
    Steinberg::FUnknown* keepAlive_ = nullptr;
    Steinberg::IPlugFrame* frame_ = nullptr;
    HWND window_ = nullptr;
    HWND editor_ = nullptr;
    HWND runButton_ = nullptr;
    HWND stopButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND examples_ = nullptr;
    HWND status_ = nullptr;
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
