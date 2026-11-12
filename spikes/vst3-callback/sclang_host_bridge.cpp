#include "sclang_host_bridge.h"
#include "sclang_host_protocol.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace dexter::sclang {
namespace {
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;
void closeSocket(Socket s) { closesocket(s); }
#else
using Socket = int;
constexpr Socket invalidSocket = -1;
void closeSocket(Socket s) { close(s); }
#endif
}
struct HostOscBridge::Impl {
    Socket socket = invalidSocket;
    std::atomic<bool> stopping{false};
    std::atomic<bool> clear{false};
    std::thread thread;
    Dispatch dispatch;
    Maintenance maintenance;
    std::mutex replyMutex;
    std::deque<HostOscScheduled> replies;
    std::size_t replyBytes = 0;
    HostOscBridge* owner = nullptr;

    void run() noexcept {
        try {
            std::array<char, 65536> buffer{};
            std::vector<HostOscScheduled> pending;
            auto clearFuture = [&] {
                const auto now = std::chrono::system_clock::now();
                pending.erase(std::remove_if(pending.begin(), pending.end(),
                    [&](const auto& m) { return m.due > now; }), pending.end());
            };
            while (!stopping.load(std::memory_order_acquire)) {
                if (clear.exchange(false)) clearFuture();
                for (unsigned count = 0; count < 16; ++count) {
                    sockaddr_in sender{};
#if defined(_WIN32)
                    int senderSize = sizeof(sender);
#else
                    socklen_t senderSize = sizeof(sender);
#endif
                    const auto n = recvfrom(socket, buffer.data(),
                        static_cast<int>(buffer.size()), 0,
                        reinterpret_cast<sockaddr*>(&sender), &senderSize);
                    if (n <= 0) break; // Non-blocking socket; no waits under engine locks.
                    if (sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;
                    const auto port = ntohs(sender.sin_port);
                    std::vector<char> packet(buffer.begin(), buffer.begin() + n);
                    std::vector<HostOscScheduled> incoming;
                    const auto now = std::chrono::system_clock::now();
                    const bool parsed = splitHostOsc(packet, port, incoming, now, now);
                    std::size_t total = packet.size();
                    for (const auto& item : pending) total += item.bytes.size();
                    if (!parsed || pending.size() + incoming.size() > kMaxHostMessages ||
                        total > 1024 * 1024) {
                        const auto failure = hostOscFailure("/bridge",
                            "Malformed packet, bundle over 30 seconds ahead, or queue limit exceeded");
                        owner->reply(port, failure.data(), failure.size());
                        continue;
                    }
                    for (auto& item : incoming) pending.push_back(std::move(item));
                    std::stable_sort(pending.begin(), pending.end(),
                        [](const auto& a, const auto& b) { return a.due < b.due; });
                }
                for (unsigned count = 0; count < 16 && !pending.empty(); ++count) {
                    if (pending.front().due > std::chrono::system_clock::now()) break;
                    if (!dispatch(pending.front().bytes, pending.front().port)) break;
                    pending.erase(pending.begin());
                    if (clear.exchange(false)) clearFuture();
                }
                if (maintenance) maintenance();
                for (unsigned count = 0; count < 32; ++count) {
                    HostOscScheduled item;
                    {
                        std::lock_guard<std::mutex> lock(replyMutex);
                        if (replies.empty()) break;
                        item = std::move(replies.front());
                        replyBytes -= item.bytes.size();
                        replies.pop_front();
                    }
                    sockaddr_in target{};
                    target.sin_family = AF_INET;
                    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    target.sin_port = htons(item.port);
                    sendto(socket, item.bytes.data(), static_cast<int>(item.bytes.size()),
                        0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        } catch (...) {
            // Never let a transport exception terminate the host process.
            stopping.store(true, std::memory_order_release);
        }
    }
};

HostOscBridge::HostOscBridge() : impl_(std::make_unique<Impl>()) { impl_->owner = this; }
HostOscBridge::~HostOscBridge() { stop(); }
bool HostOscBridge::start(int port, Dispatch dispatch, Maintenance maintenance,
                           std::string& error) {
    stop();
    if (port <= 0 || port > 65535 || !dispatch) {
        error = "Invalid bridge endpoint"; return false;
    }
#if defined(_WIN32)
    static const bool ready = [] { WSADATA w{}; return WSAStartup(MAKEWORD(2,2), &w) == 0; }();
    if (!ready) { error = "Winsock initialization failed"; return false; }
#endif
    auto& p = *impl_;
    p.socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (p.socket == invalidSocket) { error = "Could not create loopback UDP socket"; return false; }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (bind(p.socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = "Could not bind loopback UDP endpoint"; stop(); return false;
    }
#if defined(_WIN32)
    u_long nonblocking = 1;
    const bool configured = ioctlsocket(p.socket, FIONBIO, &nonblocking) == 0;
#else
    const int flags = fcntl(p.socket, F_GETFL);
    const bool configured = flags >= 0 && fcntl(p.socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    if (!configured) { error = "Could not configure non-blocking bridge"; stop(); return false; }
    p.dispatch = std::move(dispatch);
    p.maintenance = std::move(maintenance);
    p.clear.store(false);
    p.stopping.store(false);
    try { p.thread = std::thread([&p] { p.run(); }); }
    catch (...) { error = "Could not start bridge worker"; stop(); return false; }
    error.clear();
    return true;
}
void HostOscBridge::stop() {
    auto& p = *impl_;
    p.stopping.store(true, std::memory_order_release);
    if (p.thread.joinable()) p.thread.join();
    if (p.socket != invalidSocket) closeSocket(p.socket);
    p.socket = invalidSocket;
    p.dispatch = {};
    p.maintenance = {};
    std::lock_guard<std::mutex> lock(p.replyMutex);
    p.replies.clear();
    p.replyBytes = 0;
}
void HostOscBridge::reply(std::uint16_t port, const char* bytes, std::size_t size) {
    if (!port || !bytes || !size || size > kMaxHostPacketBytes) return;
    std::lock_guard<std::mutex> lock(impl_->replyMutex);
    if (impl_->replies.size() >= kMaxHostMessages ||
        impl_->replyBytes + size > 1024 * 1024) return;
    impl_->replies.push_back({std::vector<char>(bytes, bytes + size), {}, port});
    impl_->replyBytes += size;
}
void HostOscBridge::clearScheduled() { impl_->clear.store(true); }

} // namespace dexter::sclang
