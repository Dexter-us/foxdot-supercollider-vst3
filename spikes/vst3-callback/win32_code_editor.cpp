#include "win32_code_editor.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

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
constexpr int kOpenId = 106;
constexpr int kSaveId = 107;
constexpr int kSaveAsId = 108;
constexpr int kBrandId = 109;
constexpr int kViewWidth = 720;
constexpr int kViewHeight = 480;
constexpr int kMargin = 10;
constexpr int kToolbarHeight = 32;
constexpr int kStatusHeight = 24;
constexpr std::size_t kMaxScriptBytes = 256 * 1024;
constexpr std::size_t kMaxScriptCharacters = 65536;

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

        window_ = CreateWindowExW(0, kWindowClass, L"Dexter U.S FoxDot",
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
            editor_ = runButton_ = stopButton_ = openButton_ = saveButton_ = saveAsButton_ =
                status_ = brand_ = nullptr;
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
            if (LOWORD(wParam) == kEditorId && HIWORD(wParam) == EN_CHANGE) {
                if (!view->updatingEditor_) {
                    view->documentDirty_ = true;
                    view->updateSaveButton();
                    if (view->host_)
                        view->host_->setEditorSource(toUtf8(view->editorSource()));
                }
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
                    view->status_ = view->brand_ = nullptr;
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
        status_ = CreateWindowExW(0, L"EDIT", L"Ready. Open a script or edit code.",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                      ES_READONLY | ES_AUTOHSCROLL | ES_NOHIDESEL,
                                  0, 0, 300, 24, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)),
                                  moduleInstance(), nullptr);
        brand_ = CreateWindowExW(0, L"STATIC", L"Dexter U.S FoxDot  |  v0.4.9",
                                 WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
                                 0, 0, 210, kStatusHeight, window_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBrandId)),
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
        MoveWindow(saveAsButton_, kMargin + 250, buttonY, 76, kToolbarHeight - 4, TRUE);
        MoveWindow(status_, kMargin + 330, buttonY, std::max(100, width - 350),
                   kToolbarHeight - 4, TRUE);
        const int editorTop = kMargin + kToolbarHeight;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        MoveWindow(editor_, kMargin, editorTop, std::max(160, width - 2 * kMargin), editorHeight, TRUE);
        MoveWindow(brand_, std::max(kMargin, width - 220),
                   height - kStatusHeight - kMargin, 210, kStatusHeight, TRUE);
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

    void runSource() {
        if (!editor_ || !host_)
            return;
        const auto source = toUtf8(editorSource());
        const auto report = host_->checkCompatibility(source);
        if (report.rfind("No unsupported constructs detected", 0) != 0) {
            const auto message = fromUtf8(report);
            MessageBoxW(window_, message.empty() ? L"Compatibility check failed; script was not run." : message.c_str(),
                        L"FoxDot compatibility check", MB_OK | MB_ICONWARNING);
            SetWindowTextW(status_, L"Compatibility items found; script was not run. Ctrl+C copies the report.");
            return;
        }
        host_->submitCode(source);
        SetWindowTextW(status_, L"Compatibility check passed; evaluating Python...");
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
    HWND saveAsButton_ = nullptr;
    HWND status_ = nullptr;
    HWND brand_ = nullptr;
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
