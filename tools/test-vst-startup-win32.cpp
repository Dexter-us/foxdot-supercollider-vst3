// Load the actual built VST3, open its editor without an engine, then require
// audible output from a first Python Run. No separate SC server is launched.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "Scintilla.h"
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <utility>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace std::chrono_literals;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct HostApplication final : IHostApplication {
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override {
        if (!object) return kInvalidArgument;
        if (std::memcmp(iid, IHostApplication::iid, sizeof(TUID)) == 0 ||
            std::memcmp(iid, FUnknown::iid, sizeof(TUID)) == 0) {
            *object = static_cast<IHostApplication*>(this);
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API getName(String128 name) override {
        const char* value = "Dexter startup smoke host";
        int i = 0;
        for (; value[i] && i < 127; ++i) name[i] = value[i];
        name[i] = 0;
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID, TUID, void** object) override {
        if (object) *object = nullptr;
        return kNoInterface;
    }
};
static void pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
static void click(HWND window, int id) {
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
                 reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
}
struct TestCodeConsent {
    std::atomic<bool> finished{false};
    std::thread worker;
    TestCodeConsent() : worker([this] {
        while (!finished.load()) {
            EnumWindows([](HWND dialog, LPARAM) -> BOOL {
                DWORD pid = 0;
                GetWindowThreadProcessId(dialog, &pid);
                wchar_t title[128]{};
                GetWindowTextW(dialog, title, 128);
                if (pid == GetCurrentProcessId() &&
                    std::wcscmp(title, L"Unrestricted SuperCollider code") == 0)
                    PostMessageW(dialog, WM_COMMAND, IDYES, 0);
                return TRUE;
            }, 0);
            std::this_thread::sleep_for(10ms);
        }
    }) {}
    ~TestCodeConsent() { finished.store(true); worker.join(); }
};
int main(int argc, char** argv) {
    try {
        require(argc == 2 || (argc == 3 && std::strcmp(argv[2], "--sclang") == 0),
                "Expected VST3 binary path and optional --sclang");
        const bool testSclang = argc == 3;
        const auto path = std::filesystem::absolute(argv[1]);
        HMODULE module = LoadLibraryW(path.c_str());
        require(module != nullptr, "LoadLibrary failed");
        auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
        auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
        auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API *)()>(
            GetProcAddress(module, "GetPluginFactory"));
        require(init && exit && getFactory && init(), "Plugin module initialization failed");
        IPluginFactory* factory = getFactory();
        require(factory != nullptr, "Plugin factory missing");
        IComponent* component = nullptr;
        for (int i = 0; i < factory->countClasses(); ++i) {
            PClassInfo info{};
            if (factory->getClassInfo(i, &info) == kResultOk &&
                std::strcmp(info.category, kVstAudioEffectClass) == 0) {
                require(factory->createInstance(info.cid, IComponent::iid,
                    reinterpret_cast<void**>(&component)) == kResultOk, "createInstance failed");
                break;
            }
        }
        require(component != nullptr, "Audio component missing");
        HostApplication host;
        require(component->initialize(&host) == kResultOk, "Component initialize failed");
        IAudioProcessor* processor = nullptr;
        IEditController* controller = nullptr;
        require(component->queryInterface(IAudioProcessor::iid,
            reinterpret_cast<void**>(&processor)) == kResultOk, "Audio processor missing");
        require(component->queryInterface(IEditController::iid,
            reinterpret_cast<void**>(&controller)) == kResultOk, "Controller missing");
        ProcessSetup setup{};
        setup.processMode = kRealtime;
        setup.symbolicSampleSize = kSample32;
        setup.maxSamplesPerBlock = 256;
        setup.sampleRate = 48000;
        auto start = std::chrono::steady_clock::now();
        require(processor->setupProcessing(setup) == kResultOk, "Host setup failed");
        require(std::chrono::steady_clock::now() - start < 1s,
                "Host setup blocked for over one second");
        require(component->setActive(true) == kResultOk, "Activation failed");
        const auto processing = processor->setProcessing(true);
        require(processing == kResultOk || processing == kNotImplemented,
                "Processing activation failed");
        HWND parent = CreateWindowExW(0, L"STATIC", L"VST3 test host",
            WS_OVERLAPPEDWINDOW, 0, 0, 1100, 700, nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        require(parent != nullptr, "Parent window creation failed");
        IPlugView* view = controller->createView(ViewType::kEditor);
        require(view != nullptr, "createView failed");
        start = std::chrono::steady_clock::now();
        require(view->attached(parent, kPlatformTypeHWND) == kResultOk, "Editor attach failed");
        require(std::chrono::steady_clock::now() - start < 5s, "Editor attach exceeded five seconds");
        HWND window = FindWindowExW(parent, nullptr, L"DexterSCCodeEditorView", nullptr);
        require(window && GetDlgItem(window, 101), "Python editor window missing");
        float left[256]{}, right[256]{};
        float* channels[]{left, right};
        AudioBusBuffers output{};
        output.numChannels = 2;
        output.channelBuffers32 = channels;
        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = 256;
        data.numOutputs = 1;
        data.outputs = &output;
        require(processor->process(data) == kResultOk, "Idle audio returned failure");
        for (int i = 0; i < 256; ++i)
            require(left[i] == 0 && right[i] == 0, "Idle audio was not silent");
        if (!testSclang) {
        const char* score = "Clock.bpm = 120\np1 >> sine(0, dur=0.25, sus=0.2, amp=0.1)\n";
        SendMessageW(GetDlgItem(window, 101), SCI_SETTEXT, 0,
                     reinterpret_cast<LPARAM>(score));
        start = std::chrono::steady_clock::now();
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(102, BN_CLICKED),
                     reinterpret_cast<LPARAM>(GetDlgItem(window, 102)));
        require(std::chrono::steady_clock::now() - start < 5s, "Run handler blocked over five seconds");
        bool audible = false;
        const auto deadline = std::chrono::steady_clock::now() + 20s;
        while (!audible && std::chrono::steady_clock::now() < deadline) {
            pump();
            require(IsWindow(window) != FALSE, "Editor disappeared during engine startup");
            require(processor->process(data) == kResultOk, "Audio processing failed");
            for (int i = 0; i < 256; ++i) {
                require(std::isfinite(left[i]) && std::isfinite(right[i]), "Audio was non-finite");
                audible |= std::abs(left[i]) > 0.00001f || std::abs(right[i]) > 0.00001f;
            }
            std::this_thread::sleep_for(5ms);
        }
        require(audible, "First Run produced no audible engine output within twenty seconds");
        } else {
            click(window, 3201);
            HWND scEditor = GetDlgItem(window, 3202);
            require(scEditor != nullptr, "SC editor missing");
            const char* score = R"SC((
Routine({
    SynthDef(\dexterSclangIntegrationCheck, {
        Out.ar(0, SinOsc.ar(997) * 0.08 ! 2);
    }).add;
    s.sync;
    x = Synth(\dexterSclangIntegrationCheck);
}).play(SystemClock);
)
)SC";
            SendMessageW(scEditor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(score));
            {
                TestCodeConsent consent; // Accept only this test's constant trusted score.
                start = std::chrono::steady_clock::now();
                click(window, 102);
                require(std::chrono::steady_clock::now() - start < 5s,
                        "SC Run handler blocked the UI");
            }
            auto processBlock = [&] {
                pump();
                require(IsWindow(window) != FALSE, "Editor disappeared while sclang ran");
                require(processor->process(data) == kResultOk, "SC audio processing failed");
                bool audible = false;
                for (int i = 0; i < 256; ++i) {
                    require(std::isfinite(left[i]) && std::isfinite(right[i]),
                            "SC audio was non-finite");
                    audible |= std::abs(left[i]) > 0.00001f ||
                               std::abs(right[i]) > 0.00001f;
                }
                std::this_thread::sleep_for(5ms);
                return audible;
            };
            bool audible = false;
            const auto limit = std::chrono::steady_clock::now() + 30s;
            while (!audible && std::chrono::steady_clock::now() < limit)
                audible = processBlock();
            if (!audible) {
                wchar_t status[16384]{};
                GetWindowTextW(GetDlgItem(window, 104), status, 16384);
                std::fwprintf(stderr, L"SuperCollider status: %ls\n", status);
            }
            require(audible, "Bundled sclang did not return actual SC-tab audio in thirty seconds");
            // Run Python while the SC tone remains alive, then stop only SC.
            click(window, 3200);
            const char* python = "Clock.bpm = 120\np1 >> sine(0, dur=0.25, sus=0.2, amp=0.1)\n";
            SendMessageW(GetDlgItem(window, 101), SCI_SETTEXT, 0,
                         reinterpret_cast<LPARAM>(python));
            click(window, 102);
            auto settle = std::chrono::steady_clock::now() + 1s;
            while (std::chrono::steady_clock::now() < settle) processBlock();
            auto measure = [&] {
                double sum = 0, real = 0, imaginary = 0;
                unsigned samples = 0;
                const auto until = std::chrono::steady_clock::now() + 1s;
                while (std::chrono::steady_clock::now() < until) {
                    processBlock();
                    for (float sample : left) {
                        const double phase = 6.283185307179586 * 997.0 * samples / 48000.0;
                        sum += sample * sample;
                        real += sample * std::cos(phase);
                        imaginary += sample * std::sin(phase);
                        ++samples;
                    }
                }
                return std::pair<double, double>{
                    std::sqrt(sum / samples), 2.0 * std::hypot(real, imaginary) / samples};
            };
            const auto both = measure();
            require(both.second > 0.001, "Expected SC's 997 Hz tone while Python was playing");
            click(window, 3201);
            click(window, 103);
            settle = std::chrono::steady_clock::now() + 300ms;
            while (std::chrono::steady_clock::now() < settle) processBlock();
            const auto pythonOnly = measure();
            require(pythonOnly.first > 0.00001, "SC Stop also stopped/destroyed FoxDot audio");
            require(pythonOnly.second < both.second * 0.2,
                    "SC Stop failed to remove the SC tone");
            // Retire the interpreter without destroying the embedded World.
            // A new helper must obtain client 0 again, not time out on a stale
            // UDP /notify registration from the former process.
            SendMessageW(scEditor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("0.exit;\n"));
            click(window, 102);
            settle = std::chrono::steady_clock::now() + 3s;
            while (std::chrono::steady_clock::now() < settle) processBlock();
            SendMessageW(scEditor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(score));
            click(window, 102);
            bool restarted = false;
            const auto retryLimit = std::chrono::steady_clock::now() + 30s;
            while (!restarted && std::chrono::steady_clock::now() < retryLimit)
                restarted = measure().second > 0.001;
            require(restarted, "Interpreter restart did not reconnect and restore SC audio");
            click(window, 103);
            std::puts("PASS: bundled sclang class library, server handshake, SynthDef.add, sync, SC audio; Python coexistence, SC-only Stop and interpreter restart");
        }
        require(view->removed() == kResultOk, "Editor removal failed");
        require(view->attached(parent, kPlatformTypeHWND) == kResultOk, "Editor reopen failed");
        view->removed();
        view->release();
        DestroyWindow(parent);
        processor->setProcessing(false);
        component->setActive(false);
        controller->release();
        processor->release();
        component->terminate();
        component->release();
        factory->release();
        exit();
        FreeLibrary(module);
        std::puts("PASS: actual VST3 fast host setup, editor attach/reopen, silent idle, deferred engine startup and finite audible sine");
        return 0;
    } catch (const std::exception& error) {
        // The external process timeout also bounds cleanup after a failed test.
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        std::fflush(stderr);
        return 1;
    }
}
