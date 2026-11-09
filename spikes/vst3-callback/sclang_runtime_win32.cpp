#include "sclang_runtime_win32.h"
#include "sclang_osc_protocol.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

namespace dexter::sclang {
namespace {

constexpr int kFirstServerPort = 58000;
constexpr int kServerPortCount = 1000;
constexpr int kFirstLanguagePort = 59000;
constexpr int kLanguagePortCount = 1000;
constexpr std::size_t kMaxSourceBytes = 256 * 1024;
constexpr std::size_t kMaxLogTailBytes = 12 * 1024;
std::atomic<unsigned> nextServerPort{0};
std::atomic<unsigned> nextLanguagePort{0};

bool winsockReady() {
    static const bool ready = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ready;
}

bool bindProbe(SOCKET socketHandle, int port, int type) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR)
        return false;
    if (type == SOCK_STREAM && listen(socketHandle, 1) == SOCKET_ERROR)
        return false;
    return true;
}

int findAvailableUdpPort() {
    if (!winsockReady())
        return 0;
    for (int attempt = 0; attempt < kLanguagePortCount; ++attempt) {
        const unsigned sequence =
            nextLanguagePort.fetch_add(1, std::memory_order_relaxed);
        const int port = kFirstLanguagePort +
                         static_cast<int>(sequence % kLanguagePortCount);
        SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketHandle == INVALID_SOCKET)
            return 0;
        const bool available = bindProbe(socketHandle, port, SOCK_DGRAM);
        closesocket(socketHandle);
        if (available)
            return port;
    }
    return 0;
}

std::wstring quoteArgument(const std::wstring& value) {
    std::wstring result = L"\"";
    unsigned backslashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(ch);
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::string yamlQuotedPath(const std::filesystem::path& path) {
    auto value = path.generic_u8string();
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char ch : value) {
        if (ch == '"' || ch == '\\')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

bool writeTextFile(const std::filesystem::path& path,
                   const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    return output.good();
}

std::string utf8Path(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return std::string(bytes.begin(), bytes.end());
}

bool sendOsc(int port, const std::vector<char>& packet) {
    if (!winsockReady() || port <= 0 || packet.empty())
        return false;
    SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET)
        return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto sent = sendto(socketHandle, packet.data(),
                             static_cast<int>(packet.size()), 0,
                             reinterpret_cast<const sockaddr*>(&address),
                             sizeof(address));
    closesocket(socketHandle);
    return sent == static_cast<int>(packet.size());
}

std::string readTail(const std::filesystem::path& path) {
    if (path.empty())
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end <= 0)
        return {};
    const auto count = static_cast<std::streamoff>(
        std::min<std::uint64_t>(static_cast<std::uint64_t>(end),
                                kMaxLogTailBytes));
    input.seekg(end - count, std::ios::beg);
    std::string value(static_cast<std::size_t>(count), '\0');
    input.read(value.data(), count);
    value.resize(static_cast<std::size_t>(input.gcount()));
    return value;
}

} // namespace

int findAvailableServerPort() {
    if (!winsockReady())
        return 0;
    for (int attempt = 0; attempt < kServerPortCount; ++attempt) {
        const unsigned sequence =
            nextServerPort.fetch_add(1, std::memory_order_relaxed);
        const int port = kFirstServerPort +
                         static_cast<int>(sequence % kServerPortCount);
        SOCKET tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcp == INVALID_SOCKET) {
            return 0;
        }
        const bool tcpAvailable = bindProbe(tcp, port, SOCK_STREAM);
        closesocket(tcp);
        if (tcpAvailable)
            return port;
    }
    return 0;
}

Runtime::~Runtime() {
    shutdown();
}

void Runtime::submit(const std::filesystem::path& runtimeRoot,
                     int serverPort,
                     int firstUserAudioBus,
                     double sampleRate,
                     std::string source) {
    if (source.size() > kMaxSourceBytes) {
        setStatus("SuperCollider source exceeds the 256 KB safety limit.");
        return;
    }
    if (source.find('\0') != std::string::npos) {
        setStatus("SuperCollider source contains a null byte and was not sent.");
        return;
    }
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 ||
        firstUserAudioBus < 2 || firstUserAudioBus >= 1024) {
        setStatus("The embedded SuperCollider engine parameters are invalid.");
        return;
    }
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_.joinable() && shutdownRequested_) {
            // The previous World may have been restarted at a new host rate.
            // Join outside the lock before starting a fresh helper.
        } else {
            runtimeRoot_ = runtimeRoot;
            serverPort_ = serverPort;
            firstUserAudioBus_ = firstUserAudioBus;
            sampleRate_ = sampleRate;
            pendingSource_ = std::move(source);
            pendingRun_ = true;
            shutdownRequested_ = false;
            ++sourceGeneration_;
            if (!worker_.joinable())
                worker_ = std::thread(&Runtime::workerLoop, this);
            condition_.notify_one();
            status_ = "Starting pinned SuperCollider 3.15 sclang; source queued.";
            return;
        }
    }
    if (worker_.joinable())
        worker_.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        runtimeRoot_ = runtimeRoot;
        serverPort_ = serverPort;
        firstUserAudioBus_ = firstUserAudioBus;
        sampleRate_ = sampleRate;
        pendingSource_ = std::move(source);
        pendingRun_ = true;
        shutdownRequested_ = false;
        ++sourceGeneration_;
        worker_ = std::thread(&Runtime::workerLoop, this);
        condition_.notify_one();
        status_ = "Starting pinned SuperCollider 3.15 sclang; source queued.";
    }
}

void Runtime::stop() {
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRun_ = false;
        pendingSource_.clear();
        ++sourceGeneration_;
        port = languagePort_;
        status_ = port > 0
            ? "Sending Stop to the SuperCollider language process..."
            : "No SuperCollider script is running.";
    }
    if (port > 0)
        sendControl("/dexter/stop");
}

void Runtime::shutdown() {
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdownRequested_ = true;
        pendingRun_ = false;
        pendingSource_.clear();
        port = languagePort_;
    }
    condition_.notify_all();
    if (port > 0)
        sendOsc(port, makeOscNoArgs("/dexter/quit"));
    if (worker_.joinable() &&
        worker_.get_id() != std::this_thread::get_id())
        worker_.join();
    HANDLE process = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        process = static_cast<HANDLE>(processHandle_);
    }
    if (process && WaitForSingleObject(process, 1500) == WAIT_TIMEOUT) {
        TerminateProcess(process, 0);
        WaitForSingleObject(process, 1000);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (processHandle_ == process && processHandle_) {
            CloseHandle(process);
            processHandle_ = nullptr;
        }
        languagePort_ = 0;
        if (!workingDirectory_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(workingDirectory_, ignored);
        }
        workingDirectory_.clear();
        logPath_.clear();
        status_ = "SuperCollider language process stopped.";
    }
}

std::string Runtime::status() const {
    std::string base;
    std::filesystem::path log;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        base = status_;
        log = logPath_;
    }
    const auto tail = readTail(log);
    if (tail.find("ERROR:") != std::string::npos ||
        tail.find("Error") != std::string::npos ||
        tail.find("Syntax error") != std::string::npos ||
        tail.find("FAILURE") != std::string::npos)
        return base + "\r\n" + tail;
    return base;
}

void Runtime::setStatus(std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = std::move(value);
}

bool Runtime::ensureStarted(const std::filesystem::path& runtimeRoot,
                            int serverPort,
                            int firstUserAudioBus,
                            double sampleRate) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (processHandle_ &&
            WaitForSingleObject(static_cast<HANDLE>(processHandle_), 0) == WAIT_TIMEOUT)
            return true;
        if (processHandle_) {
            CloseHandle(static_cast<HANDLE>(processHandle_));
            processHandle_ = nullptr;
            languagePort_ = 0;
        }
    }
    return startProcess(runtimeRoot, serverPort, firstUserAudioBus, sampleRate);
}

bool Runtime::startProcess(const std::filesystem::path& runtimeRoot,
                           int serverPort,
                           int firstUserAudioBus,
                           double sampleRate) {
    const auto executable = runtimeRoot / "sclang.exe";
    const auto classLibrary = runtimeRoot / "SCClassLibrary";
    const auto versionPath = runtimeRoot / "SC_RUNTIME_VERSION.txt";
    std::error_code error;
    if (!std::filesystem::is_regular_file(executable, error) ||
        !std::filesystem::is_directory(classLibrary, error) ||
        !std::filesystem::is_regular_file(versionPath, error)) {
        setStatus("Bundled SuperCollider 3.15 sclang runtime is missing or incomplete. Reinstall the matching VST bundle.");
        return false;
    }
    std::ifstream versionFile(versionPath, std::ios::binary);
    const std::string versionText(
        (std::istreambuf_iterator<char>(versionFile)),
        std::istreambuf_iterator<char>());
    if (versionText.find("Version: 3.15") == std::string::npos ||
        versionText.find("Source commit: ef5771bce3d1c9c780cafd58cf032ec4a3f36d10") ==
            std::string::npos ||
        versionText.find("API generation: 7") == std::string::npos ||
        versionText.find("Build option: SCLANG_SERVER=OFF") ==
            std::string::npos) {
        setStatus("Bundled sclang version manifest does not match pinned SuperCollider 3.15/API-7.");
        return false;
    }
    if (serverPort <= 0) {
        setStatus("The embedded SuperCollider loopback endpoint is unavailable.");
        return false;
    }
    const int languagePort = findAvailableUdpPort();
    if (languagePort <= 0) {
        setStatus("Could not reserve a loopback port for sclang.");
        return false;
    }

    std::array<wchar_t, MAX_PATH + 1> tempPath{};
    const DWORD tempLength = GetTempPathW(
        static_cast<DWORD>(tempPath.size()), tempPath.data());
    if (tempLength == 0 || tempLength >= tempPath.size()) {
        setStatus("Could not locate a temporary folder for sclang source and logs.");
        return false;
    }
    const auto directory = std::filesystem::path(
        std::wstring(tempPath.data(), tempLength)) /
        (L"Dexter-SC-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(GetTickCount64()));
    if (!std::filesystem::create_directories(directory, error) && error) {
        setStatus("Could not create the temporary sclang folder: " + error.message());
        return false;
    }
    const auto configPath = directory / L"sclang_conf.yaml";
    const auto bootstrapPath = directory / L"bootstrap.scd";
    const auto logPath = directory / L"sclang.log";
    const auto config = std::string(
        "includePaths:\n  - ") + yamlQuotedPath(classLibrary) +
        "\nexcludePaths: []\nexcludeDefaultPaths: true\n";
    const auto bootstrap =
        "(\n"
        "var opts = ServerOptions.new;\n"
        "opts.numInputBusChannels = 0;\n"
        "opts.numOutputBusChannels = 2;\n"
        "opts.reservedNumAudioBusChannels = " +
        std::to_string(firstUserAudioBus - 2) +
        ";\n"
        "opts.numAudioBusChannels = 1024;\n"
        "opts.sampleRate = " + std::to_string(sampleRate) + ";\n"
        "opts.protocol = \\tcp;\n"
        "s = Server.new(\\DexterVST, NetAddr(\"127.0.0.1\", " +
        std::to_string(serverPort) +
        "), opts);\n"
        "Server.default = s;\n"
        "s.connectToServerAddr({ s.startAliveThread });\n"
        "OSCdef(\\DexterVSTRun, { |msg, time, addr| "
        "if(addr.ip == \"127.0.0.1\") { msg[1].asString.load; }; }, "
        "\"/dexter/evalFile\");\n"
        "OSCdef(\\DexterVSTStop, { |msg, time, addr| "
        "if(addr.ip == \"127.0.0.1\") { CmdPeriod.run; s.freeAll; }; }, "
        "\"/dexter/stop\");\n"
        "OSCdef(\\DexterVSTQuit, { |msg, time, addr| "
        "if(addr.ip == \"127.0.0.1\") { 0.exit; }; }, "
        "\"/dexter/quit\");\n"
        "s.doWhenBooted({ \"DEXTER_SCLANG_READY\".postln; });\n"
        ")\n";
    if (!writeTextFile(configPath, config) ||
        !writeTextFile(bootstrapPath, bootstrap)) {
        setStatus("Could not write the sclang startup files.");
        return false;
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE logFile = CreateFileW(logPath.c_str(), GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 &security, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE) {
        setStatus("Could not create the sclang output log.");
        return false;
    }
    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        CloseHandle(logFile);
        setStatus("Could not create a non-interactive sclang input handle.");
        return false;
    }

    std::wstring commandLine =
        quoteArgument(executable.wstring()) + L" -D -u " +
        std::to_wstring(languagePort) + L" -l " +
        quoteArgument(configPath.wstring()) + L" " +
        quoteArgument(bootstrapPath.wstring());
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullInput;
    startup.hStdOutput = logFile;
    startup.hStdError = logFile;
    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr,
        runtimeRoot.c_str(), &startup, &processInfo);
    CloseHandle(nullInput);
    CloseHandle(logFile);
    if (!created) {
        setStatus("Could not launch bundled sclang.exe (Windows error " +
                  std::to_string(GetLastError()) + ").");
        return false;
    }
    CloseHandle(processInfo.hThread);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        processHandle_ = processInfo.hProcess;
        languagePort_ = languagePort;
        workingDirectory_ = directory;
        logPath_ = logPath;
    }

    setStatus("sclang 3.15 is loading its class library and connecting to the embedded VST engine...");
    constexpr auto timeout = std::chrono::seconds(60);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        bool cancelled = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled = shutdownRequested_;
        }
        if (cancelled) {
            TerminateProcess(processInfo.hProcess, 0);
            WaitForSingleObject(processInfo.hProcess, 1000);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (processHandle_ == processInfo.hProcess) {
                    CloseHandle(processInfo.hProcess);
                    processHandle_ = nullptr;
                    languagePort_ = 0;
                }
            }
            setStatus("sclang startup cancelled.");
            return false;
        }
        if (WaitForSingleObject(processInfo.hProcess, 0) != WAIT_TIMEOUT) {
            setStatus("sclang exited before the embedded server connected.\r\n" +
                      readTail(logPath));
            return false;
        }
        const auto tail = readTail(logPath);
        if (tail.find("DEXTER_SCLANG_READY") != std::string::npos) {
            setStatus("sclang 3.15 connected to the VST's embedded SuperCollider engine.");
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    TerminateProcess(processInfo.hProcess, 0);
    WaitForSingleObject(processInfo.hProcess, 1000);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (processHandle_ == processInfo.hProcess) {
            CloseHandle(processInfo.hProcess);
            processHandle_ = nullptr;
            languagePort_ = 0;
        }
    }
    setStatus("Timed out waiting for sclang to connect to the VST's embedded engine.\r\n" +
              readTail(logPath));
    return false;
}

bool Runtime::sendSource(const std::string& source) {
    int port = 0;
    std::filesystem::path directory;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        port = languagePort_;
        directory = workingDirectory_;
        generation = sourceGeneration_;
    }
    if (port <= 0 || directory.empty())
        return false;
    const auto sourcePath = directory /
        (L"script-" + std::to_wstring(generation) + L".scd");
    const auto temporaryPath = sourcePath.wstring() + L".tmp";
    if (!writeTextFile(temporaryPath, source))
        return false;
    std::error_code error;
    std::filesystem::rename(temporaryPath, sourcePath, error);
    if (error) {
        std::filesystem::remove(sourcePath, error);
        error.clear();
        std::filesystem::rename(temporaryPath, sourcePath, error);
    }
    if (error)
        return false;
    const auto packet = makeOscString("/dexter/evalFile", utf8Path(sourcePath));
    return sendOsc(port, packet);
}

bool Runtime::sendControl(const char* address) {
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        port = languagePort_;
    }
    return sendOsc(port, makeOscNoArgs(address));
}

void Runtime::workerLoop() {
    for (;;) {
        std::filesystem::path runtimeRoot;
        int serverPort = 0;
        int firstUserAudioBus = 2;
        double sampleRate = 44100.0;
        std::string source;
        std::uint64_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return shutdownRequested_ || pendingRun_;
            });
            if (shutdownRequested_ && !pendingRun_)
                break;
            source = std::move(pendingSource_);
            pendingSource_.clear();
            pendingRun_ = false;
            runtimeRoot = runtimeRoot_;
            serverPort = serverPort_;
            firstUserAudioBus = firstUserAudioBus_;
            sampleRate = sampleRate_;
            generation = sourceGeneration_;
        }
        if (!ensureStarted(runtimeRoot, serverPort, firstUserAudioBus,
                           sampleRate))
            continue;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdownRequested_ || generation != sourceGeneration_)
                continue;
        }
        if (source.empty())
            continue;
        if (sendSource(source))
            setStatus("SuperCollider source sent to the VST's embedded 3.15 engine.");
        else
            setStatus("Could not send the .scd source to sclang over loopback OSC.");
    }
}

} // namespace dexter::sclang

#else

#include <atomic>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace dexter::sclang {

int findAvailableServerPort() {
    static std::atomic<unsigned> nextPort{0};
    constexpr int firstPort = 58000;
    constexpr unsigned portCount = 1000;
    for (unsigned attempt = 0; attempt < portCount; ++attempt) {
        const int port = firstPort + static_cast<int>(
            nextPort.fetch_add(1, std::memory_order_relaxed) % portCount);
        const int tcp = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp < 0) {
            return 0;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const bool tcpAvailable =
            bind(tcp, reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address)) == 0 &&
            listen(tcp, 1) == 0;
        close(tcp);
        if (tcpAvailable)
            return port;
    }
    return 0;
}

Runtime::~Runtime() = default;
void Runtime::submit(const std::filesystem::path&, int, int, double,
                     std::string) {
    setStatus("The bundled sclang tab is available in the Windows VST3 build.");
}
void Runtime::stop() {}
void Runtime::shutdown() {}
std::string Runtime::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}
void Runtime::setStatus(std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = std::move(value);
}
bool Runtime::ensureStarted(const std::filesystem::path&, int, int, double) { return false; }
bool Runtime::startProcess(const std::filesystem::path&, int, int, double) { return false; }
bool Runtime::sendSource(const std::string&) { return false; }
bool Runtime::sendControl(const char*) { return false; }
void Runtime::workerLoop() {}

} // namespace dexter::sclang

#endif
