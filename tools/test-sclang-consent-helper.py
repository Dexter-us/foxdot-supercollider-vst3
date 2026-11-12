#!/usr/bin/env python3
"""Exercise the actual consent helper with a simulated nonresponsive UI.

This is a Win32 API model, not a native Windows/editor validation. The real
Windows integration test remains required before publishing a VST artifact.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "tools/test-vst-startup-win32.cpp").read_text()
start = source.index("struct TestCodeConsent {")
helper = source[start:source.index("\nint main(", start)]
code = r"""
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
using HWND = int;
using LPARAM = std::intptr_t;
using DWORD = unsigned long;
using DWORD_PTR = std::uintptr_t;
using BOOL = int;
constexpr int TRUE = 1, WM_GETTEXT = 13, WM_COMMAND = 273, IDYES = 6;
constexpr unsigned SMTO_ABORTIFHUNG = 2, SMTO_BLOCK = 1;
std::atomic<int> timedCalls{0}, unboundedCalls{0}, normalWindowQueries{0};
DWORD GetCurrentProcessId() { return 42; }
void GetWindowThreadProcessId(HWND, DWORD* pid) { *pid = 42; }
void GetClassNameW(HWND window, wchar_t* out, int) {
    std::wcscpy(out, window == 1 ? L"HostWindow" : L"#32770");
}
int GetWindowTextW(HWND, wchar_t*, int) {
    ++unboundedCalls;
    throw std::runtime_error("Unbounded same-process UI call would deadlock");
}
int SendMessageTimeoutW(HWND window, int message, int, LPARAM,
                       unsigned flags, unsigned timeout, DWORD_PTR*) {
    if (window == 1) ++normalWindowQueries;
    assert(message == WM_GETTEXT);
    assert((flags & SMTO_ABORTIFHUNG) && (flags & SMTO_BLOCK));
    assert(timeout > 0 && timeout <= 100);
    ++timedCalls;
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    return 0; // The UI is joining this helper and cannot answer.
}
void PostMessageW(HWND, int, int, int) {}
void EnumWindows(BOOL (*callback)(HWND, LPARAM), LPARAM data) {
    callback(1, data);
    callback(2, data);
}
"""
code += helper
code += r"""
int main() {
    const auto start = std::chrono::steady_clock::now();
    {
        TestCodeConsent helper;
        std::this_thread::sleep_for(30ms);
    }
    assert(std::chrono::steady_clock::now() - start < 600ms);
    assert(timedCalls.load() > 0);
    assert(unboundedCalls.load() == 0);
    assert(normalWindowQueries.load() == 0);
}
"""
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "consent.cpp"
    binary = Path(directory) / "consent"
    cpp.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-pthread", "-fsanitize=undefined",
                    "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True, timeout=3)
print("PASS: actual helper joins with an unresponsive UI, uses bounded dialog-only queries")
print("Scope: modeled Win32 calls; native Windows integration remains required.")
