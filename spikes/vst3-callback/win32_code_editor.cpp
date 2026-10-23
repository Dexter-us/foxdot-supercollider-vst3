#include "win32_code_editor.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr wchar_t kWindowClass[] = L"DexterSCCodeEditorView";
constexpr int kEditorId = 101;
constexpr int kRunId = 102;
constexpr int kStopId = 103;
constexpr int kStatusId = 104;
constexpr int kViewWidth = 720;
constexpr int kViewHeight = 480;
constexpr int kMargin = 10;
constexpr int kToolbarHeight = 32;
constexpr int kStatusHeight = 24;

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
    const int required = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0);
    if (required <= 0)
        return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
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
            DestroyWindow(window_);
            window_ = nullptr;
            editor_ = runButton_ = stopButton_ = status_ = nullptr;
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
            break;
        case WM_TIMER:
            view->refreshStatus();
            return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            view->window_ = nullptr;
            view->editor_ = view->runButton_ = view->stopButton_ = view->status_ = nullptr;
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void createChildren() {
        runButton_ = CreateWindowExW(0, L"BUTTON", L"Run",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     0, 0, 80, 26, window_,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRunId)),
                                     moduleInstance(), nullptr);
        stopButton_ = CreateWindowExW(0, L"BUTTON", L"Stop",
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      0, 0, 80, 26, window_,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStopId)),
                                      moduleInstance(), nullptr);
        status_ = CreateWindowExW(0, L"STATIC", L"Ready. Use tempo() and play().",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  0, 0, 300, 24, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)),
                                  moduleInstance(), nullptr);
        editor_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                  L"tempo(120)\r\nplay([60, 64, 67, 72], step=0.5, duration=0.45)\r\n",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
                                      ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
                                  0, 0, 600, 380, window_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorId)),
                                  moduleInstance(), nullptr);
        SendMessageW(editor_, EM_SETLIMITTEXT, 65536, 0);
        SendMessageW(editor_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
        SetTimer(window_, 1, 250, nullptr);
        layoutChildren(kViewWidth, kViewHeight);
    }

    void layoutChildren(int width, int height) {
        if (!editor_)
            return;
        const int buttonY = kMargin;
        MoveWindow(runButton_, kMargin, buttonY, 80, kToolbarHeight - 4, TRUE);
        MoveWindow(stopButton_, kMargin + 88, buttonY, 80, kToolbarHeight - 4, TRUE);
        MoveWindow(status_, kMargin + 180, buttonY, std::max(100, width - 190), kToolbarHeight - 4, TRUE);
        const int editorTop = kMargin + kToolbarHeight;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        MoveWindow(editor_, kMargin, editorTop, std::max(160, width - 2 * kMargin), editorHeight, TRUE);
    }

    void runSource() {
        if (!editor_ || !host_)
            return;
        const int length = GetWindowTextLengthW(editor_);
        std::wstring source(static_cast<std::size_t>(length) + 1, L'\0');
        if (length > 0)
            GetWindowTextW(editor_, source.data(), length + 1);
        source.resize(static_cast<std::size_t>(length));
        host_->submitCode(toUtf8(source));
        SetWindowTextW(status_, L"Evaluating Python...");
    }

    void refreshStatus() {
        if (!host_ || !status_)
            return;
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
    HWND status_ = nullptr;
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
