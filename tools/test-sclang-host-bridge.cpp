#include "../spikes/vst3-callback/sclang_host_protocol.h"
#include "../spikes/vst3-callback/sclang_host_bridge.h"
#include <cstdio>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace dexter::sclang;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static std::vector<char> ints(const char* command, std::initializer_list<int> values) {
    auto p = hostOscMessage(command, "," + std::string(values.size(), 'i'));
    for (int n : values) appendOscU32(p, n);
    return p;
}
static std::vector<char> bundle(const std::vector<char>& message) {
    std::vector<char> p;
    appendOscString(p, "#bundle");
    appendOscU32(p, 0);
    appendOscU32(p, 1);
    appendOscU32(p, static_cast<std::uint32_t>(message.size()));
    p.insert(p.end(), message.begin(), message.end());
    return p;
}
int main(int argc, char**) {
    try {
        HostOscMessage m;
        auto status = makeOscNoArgs("/status");
        require(protectHostCommand(status, m).empty(), "Status command rejected");
        auto freeAll = ints("/g_freeAll", {0});
        require(protectHostCommand(freeAll, m).empty() &&
            m.args[0].integer(freeAll) == 1, "Root free was not isolated to the SC group");
        auto privateFree = ints("/n_free", {101});
        require(!protectHostCommand(privateFree, m).empty(), "FoxDot node free was allowed");
        auto rootFree = ints("/n_free", {0});
        require(!protectHostCommand(rootFree, m).empty(), "Root node free was allowed");
        auto rootPause = ints("/n_run", {0, 0});
        require(!protectHostCommand(rootPause, m).empty(), "Root pause was allowed");
        auto group = ints("/g_new", {1, 0, 0});
        require(protectHostCommand(group, m).empty(), "Default group creation rejected");
        auto replace = ints("/g_new", {kLanguageFirstNodeId, 4, 100});
        require(!protectHostCommand(replace, m).empty(), "Private node replacement was allowed");
        auto ownFree = ints("/n_free", {kLanguageFirstNodeId});
        require(protectHostCommand(ownFree, m).empty(), "Language node free rejected");
        auto numericFree = ints("/n_free", {100});
        const auto tags = std::find(numericFree.begin(), numericFree.end(), ',');
        std::vector<char> compact;
        appendOscU32(compact, 11);
        compact.insert(compact.end(), tags, numericFree.end());
        require(!protectHostCommand(compact, m).empty(),
                "Numeric n_free bypassed private-node protection");
        compact.clear();
        appendOscU32(compact, 9);
        appendOscString(compact, ",siiii");
        appendOscString(compact, "test");
        for (int n : {kLanguageFirstNodeId, 0, 1, 0}) appendOscU32(compact, n);
        require(protectHostCommand(compact, m).empty(),
                "Numeric s_new with nil-control sentinel was rejected");
        auto badControl = hostOscMessage("/n_set", ",i");
        appendOscU32(badControl, kLanguageFirstNodeId);
        require(!protectHostCommand(badControl, m).empty(),
                "Incomplete control pairs accepted");
        auto nonFinite = hostOscMessage("/n_set", ",isf");
        appendOscU32(nonFinite, kLanguageFirstNodeId);
        appendOscString(nonFinite, "freq");
        appendOscU32(nonFinite, 0x7fc00000);
        require(!protectHostCommand(nonFinite, m).empty(), "NaN control accepted");
        auto quit = makeOscNoArgs("/quit");
        require(!protectHostCommand(quit, m).empty(), "Engine quit was allowed");
        auto malformed = ints("/sync", {1});
        malformed.pop_back();
        require(!parseHostOsc(malformed, m), "Truncated argument accepted");
        auto blob = hostOscMessage("/d_recv", ",b");
        appendOscU32(blob, 0xffffffffu);
        require(!parseHostOsc(blob, m), "Oversized blob accepted");
        auto now = std::chrono::system_clock::now();
        std::vector<HostOscScheduled> messages;
        require(splitHostOsc(bundle(status), 1, messages, now, now) &&
            messages.size() == 1, "Immediate bundle failed");
        auto deep = status;
        for (int i = 0; i < 10; ++i) deep = bundle(deep);
        messages.clear();
        require(!splitHostOsc(deep, 1, messages, now, now), "Deep bundle was accepted");
        auto future = bundle(status);
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count() + 2208988800LL + 60;
        replaceOscInt(future, 8, static_cast<int>(seconds));
        messages.clear();
        require(!splitHostOsc(future, 1, messages, now, now), "Unbounded future event accepted");
        HostOscBridge bridge;
        std::atomic<int> cancelled{0}, barrier{0}, retried{0};
        std::string error;
        int port = 58000;
        for (; port < 59000; ++port) {
            if (bridge.start(port, [&](std::vector<char>& packet, std::uint16_t sender) {
                HostOscMessage parsed;
                const auto invalid = protectHostCommand(packet, parsed);
                if (argc == 1 && parsed.address == "/sync" && invalid.empty()) {
                    const auto id = parsed.args[0].integer(packet);
                    if (id == -1001) { ++cancelled; return true; }
                    if (id == -1002) { ++barrier; return true; }
                    if (id == -1003) return ++retried > 2; // Busy-world retry.
                }
                std::printf("COMMAND %s %s\n", parsed.address.c_str(), invalid.c_str());
                std::vector<char> response;
                if (!invalid.empty()) response = hostOscFailure(parsed.address, invalid);
                else if (parsed.address == "/status") {
                    response = hostOscMessage("/status.reply", ",iiiiiffdd");
                    for (int n : {1, 0, 0, 1, 1}) appendOscU32(response, n);
                    appendOscFloat(response, 0); appendOscFloat(response, 0);
                    appendOscDouble(response, 48000); appendOscDouble(response, 48000);
                } else if (parsed.address == "/notify") {
                    response = hostOscMessage("/done", ",sii");
                    appendOscString(response, "/notify");
                    appendOscU32(response, 0); appendOscU32(response, 1);
                } else if (parsed.address == "/sync") {
                    response = hostOscMessage("/synced", ",i");
                    appendOscU32(response, parsed.args[0].integer(packet));
                } else if (parsed.address == "/d_recv") {
                    const auto& b = parsed.args[0];
                    require(b.size >= 10 && std::memcmp(packet.data() + b.offset, "SCgf", 4) == 0,
                            "Actual sclang did not send SynthDef bytecode");
                    std::puts("SYNTHDEF_BYTECODE_RECEIVED");
                    response = hostOscMessage("/done", ",s");
                    appendOscString(response, "/d_recv");
                } else if (parsed.address == "/s_new") {
                    std::puts("LANGUAGE_SYNTH_REQUEST_RECEIVED");
                } else if (parsed.address == "/clearSched") bridge.clearScheduled();
                if (!response.empty()) bridge.reply(sender, response.data(), response.size());
                std::fflush(stdout);
                return true;
            }, {}, error)) break;
        }
        require(port < 59000, "Could not bind test bridge");
        if (argc == 1) {
            auto wait = [&](auto condition, const char* text) {
                const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (!condition() && std::chrono::steady_clock::now() < until)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                require(condition(), text);
            };
            // Loop back through the actual socket; no direct callback shortcut.
            auto scheduled = bundle(ints("/sync", {-1001}));
            const double stamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()).count() + 0.8;
            const auto whole = static_cast<std::uint64_t>(stamp);
            replaceOscInt(scheduled, 8, static_cast<int>(whole + 2208988800ULL));
            replaceOscInt(scheduled, 12,
                static_cast<int>(static_cast<std::uint32_t>((stamp - whole) * 4294967296.0)));
            auto immediate = ints("/sync", {-1002});
            bridge.reply(port, scheduled.data(), scheduled.size());
            bridge.reply(port, immediate.data(), immediate.size());
            wait([&] { return barrier.load() == 1; }, "Immediate UDP barrier did not dispatch");
            require(cancelled.load() == 0, "Future bundle dispatched early");
            bridge.clearScheduled();
            auto busy = ints("/sync", {-1003});
            bridge.reply(port, busy.data(), busy.size());
            wait([&] { return retried.load() == 3; }, "Busy-world command was dropped, not retried");
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            require(cancelled.load() == 0, "SC Stop did not clear future bundles");
        }
        if (argc > 1) {
            std::printf("FIXTURE_PORT=%d\n", port); std::fflush(stdout);
            std::string line;
            std::getline(std::cin, line);
        }
        bridge.stop();
        // Rebinding after shutdown must not leave a stale listener/thread.
        require(bridge.start(port, [](auto&, auto) { return true; }, {}, error),
                "Bridge could not restart on its released port");
        bridge.stop();
        std::puts("PASS: bounded OSC parsing, scheduled bundles, private-node protection and transport lifecycle");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
