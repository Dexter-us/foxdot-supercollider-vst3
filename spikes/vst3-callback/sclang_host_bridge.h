#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dexter::sclang {

// Loopback UDP transport. No engine operation or socket wait occurs on the UI
// or audio thread. Dispatch must try-lock the engine and return false if busy.
class HostOscBridge {
public:
    using Dispatch = std::function<bool(std::vector<char>&, std::uint16_t)>;
    using Maintenance = std::function<void()>;
    HostOscBridge();
    ~HostOscBridge();
    bool start(int port, Dispatch dispatch, Maintenance maintenance, std::string& error);
    void stop();
    void reply(std::uint16_t port, const char* bytes, std::size_t size);
    void clearScheduled();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dexter::sclang
