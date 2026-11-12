#pragma once

#include "sclang_osc_protocol.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace dexter::sclang {

inline constexpr int kLanguageFirstNodeId = 0x01000000;
inline constexpr std::size_t kMaxHostPacketBytes = 65504;
inline constexpr std::size_t kMaxHostMessages = 256;
inline constexpr std::size_t kMaxLanguageSynthDefs = 256;

inline std::uint32_t oscU32(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) |
           (std::uint32_t(b[2]) << 8) | b[3];
}
inline void appendOscU32(std::vector<char>& p, std::uint32_t n) {
    for (int shift = 24; shift >= 0; shift -= 8)
        p.push_back(static_cast<char>((n >> shift) & 255));
}
inline void replaceOscInt(std::vector<char>& p, std::size_t offset, int n) {
    for (int i = 0; i < 4; ++i)
        p[offset + i] = static_cast<char>((std::uint32_t(n) >> (24 - 8 * i)) & 255);
}
inline void appendOscFloat(std::vector<char>& p, float n) {
    std::uint32_t bits;
    std::memcpy(&bits, &n, sizeof(bits));
    appendOscU32(p, bits);
}
inline void appendOscDouble(std::vector<char>& p, double n) {
    std::uint64_t bits;
    std::memcpy(&bits, &n, sizeof(bits));
    appendOscU32(p, static_cast<std::uint32_t>(bits >> 32));
    appendOscU32(p, static_cast<std::uint32_t>(bits));
}
inline std::vector<char> hostOscMessage(std::string_view address,
                                        std::string_view tags) {
    std::vector<char> result;
    appendOscString(result, address);
    appendOscString(result, tags);
    return result;
}
inline std::vector<char> hostOscFailure(std::string_view command,
                                       std::string_view reason) {
    auto reply = hostOscMessage("/fail", ",ss");
    appendOscString(reply, command);
    appendOscString(reply, reason);
    return reply;
}

struct HostOscArg {
    char type = 0;
    std::size_t offset = 0, size = 0;
    int integer(const std::vector<char>& bytes) const {
        return static_cast<std::int32_t>(oscU32(bytes.data() + offset));
    }
    std::string text(const std::vector<char>& bytes) const {
        return {bytes.data() + offset, size};
    }
};
struct HostOscMessage {
    std::string address;
    std::vector<HostOscArg> args;
};

// Validate lengths, padding and argument types before SC's unchecked OSC reader.
inline bool parseHostOsc(const std::vector<char>& p, HostOscMessage& message) {
    message = {};
    if (p.empty() || p.size() > kMaxHostPacketBytes || (p.size() & 3))
        return false;
    std::size_t pos = 0;
    auto string = [&](HostOscArg& arg) {
        const auto begin = pos;
        while (pos < p.size() && p[pos]) ++pos;
        if (pos == p.size()) return false;
        arg.offset = begin;
        arg.size = pos - begin;
        const auto end = (pos + 4) & ~std::size_t(3);
        if (end > p.size()) return false;
        while (pos < end) if (p[pos++] != 0) return false;
        return true;
    };
    HostOscArg address, types;
    if (p.size() >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0) {
        // SC's own Node classes send numeric command IDs, e.g. 9 for s_new.
        switch (static_cast<unsigned char>(p[3])) {
        case 1: message.address = "/notify"; break;
        case 2: message.address = "/status"; break;
        case 3: message.address = "/quit"; break;
        case 5: message.address = "/d_recv"; break;
        case 9: message.address = "/s_new"; break;
        case 11: message.address = "/n_free"; break;
        case 12: message.address = "/n_run"; break;
        case 15: message.address = "/n_set"; break;
        case 21: message.address = "/g_new"; break;
        case 24: message.address = "/g_freeAll"; break;
        case 50: message.address = "/g_deepFree"; break;
        case 51: message.address = "/clearSched"; break;
        case 52: message.address = "/sync"; break;
        case 64: message.address = "/version"; break;
        default: message.address = "/unsupported-command-index"; break;
        }
        pos = 4;
    } else {
        if (!string(address)) return false;
        message.address = address.text(p);
    }
    if (!string(types)) return false;
    const auto tags = types.text(p);
    if (message.address.empty() || message.address[0] != '/' ||
        tags.empty() || tags[0] != ',' || tags.size() > 513) return false;
    for (std::size_t i = 1; i < tags.size(); ++i) {
        HostOscArg arg;
        arg.type = tags[i];
        arg.offset = pos;
        if (arg.type == 's') {
            if (!string(arg)) return false;
        } else if (arg.type == 'b') {
            if (p.size() - pos < 4) return false;
            arg.size = oscU32(p.data() + pos);
            pos += 4;
            arg.offset = pos;
            if (arg.size > p.size() - pos) return false;
            pos += arg.size;
            const auto padded = (pos + 3) & ~std::size_t(3);
            if (padded > p.size()) return false;
            while (pos < padded) if (p[pos++] != 0) return false;
        } else {
            arg.size = (arg.type == 'i' || arg.type == 'f') ? 4 :
                       (arg.type == 'd' || arg.type == 'h') ? 8 : 0;
            if (!arg.size || arg.size > p.size() - pos) return false;
            pos += arg.size;
        }
        message.args.push_back(arg);
    }
    return pos == p.size();
}

struct HostOscScheduled {
    std::vector<char> bytes;
    std::chrono::system_clock::time_point due;
    std::uint16_t port = 0;
};

// OSC bundles are flattened transactionally, preserving equal-time order.
// This bridge schedules to host-block boundaries, not sample-accurate bundles.
inline bool splitHostOsc(const std::vector<char>& bytes,
                         std::uint16_t port,
                         std::vector<HostOscScheduled>& result,
                         std::chrono::system_clock::time_point now,
                         std::chrono::system_clock::time_point inherited,
                         unsigned depth = 0) {
    if (depth > 8 || bytes.empty() || bytes.size() > kMaxHostPacketBytes ||
        (bytes.size() & 3) || result.size() >= kMaxHostMessages) return false;
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), "#bundle\0", 8) == 0) {
        if (bytes.size() < 16) return false;
        const auto seconds = oscU32(bytes.data() + 8);
        const auto fraction = oscU32(bytes.data() + 12);
        auto due = inherited;
        if (!(seconds == 0 && fraction == 1)) {
            const double unixSeconds = static_cast<double>(seconds) - 2208988800.0 +
                static_cast<double>(fraction) / 4294967296.0;
            const auto stamp = std::chrono::system_clock::time_point{
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::duration<double>(unixSeconds))};
            due = std::max(inherited, stamp);
            if (due > now + std::chrono::seconds(30)) return false;
        }
        std::size_t offset = 16;
        while (offset < bytes.size()) {
            if (bytes.size() - offset < 4) return false;
            const auto n = oscU32(bytes.data() + offset);
            offset += 4;
            if (!n || n > bytes.size() - offset) return false;
            std::vector<char> child(bytes.begin() + offset, bytes.begin() + offset + n);
            if (!splitHostOsc(child, port, result, now, due, depth + 1)) return false;
            offset += n;
        }
        return true;
    }
    HostOscMessage parsed;
    if (!parseHostOsc(bytes, parsed)) return false;
    result.push_back({bytes, inherited, port});
    return true;
}

inline bool languageNode(int id) {
    return id == 1 || id >= kLanguageFirstNodeId;
}

// Restrict the first bridge to commands whose NRT behavior we have inspected.
// In particular /status cannot call AudioStatusCmd (it dereferences a driver),
// and root frees/replacements must never invalidate FoxDot's cached Graph*s.
inline std::string protectHostCommand(std::vector<char>& bytes,
                                      HostOscMessage& m) {
    if (!parseHostOsc(bytes, m)) return "Malformed OSC message";
    auto integer = [&](std::size_t i) {
        return i < m.args.size() && m.args[i].type == 'i';
    };
    auto node = [&](std::size_t i) {
        return integer(i) && languageNode(m.args[i].integer(bytes));
    };
    auto numeric = [&](const HostOscArg& arg) {
        if (arg.type == 'i') return true;
        if (arg.type != 'f') return false;
        const auto bits = oscU32(bytes.data() + arg.offset);
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value);
    };
    const auto& a = m.address;
    if (a == "/status" || a == "/version" || a == "/clearSched")
        return m.args.empty() ? "" : "Command takes no arguments";
    if (a == "/sync")
        return m.args.size() == 1 && integer(0) ? "" : "Expected one sync ID";
    if (a == "/notify")
        return !m.args.empty() && m.args.size() <= 2 && integer(0) &&
               (m.args[0].integer(bytes) == 0 || m.args[0].integer(bytes) == 1) &&
               (m.args.size() == 1 || (integer(1) && m.args[1].integer(bytes) == 0))
            ? "" : "Only the bundled language client is supported";
    if (a == "/d_recv")
        return !m.args.empty() && m.args[0].type == 'b' && m.args[0].size >= 10 &&
            (m.args.size() == 1 || (m.args.size() == 2 &&
                ((m.args[1].type == 'i' && m.args[1].integer(bytes) == 0) ||
                 (m.args[1].type == 'b' && m.args[1].size == 0))))
            ? "" : "Expected one SynthDef blob; completion messages are not supported";
    if (a == "/s_new") {
        auto end = m.args.size();
        // Synth.new(..., args:nil) emits a trailing integer zero on the wire.
        if ((end & 1) && m.args.back().type == 'i' &&
            m.args.back().integer(bytes) == 0) --end;
        if (end < 4 || (end & 1) ||
            m.args[0].type != 's' || !integer(1) || !integer(2) || !node(3) ||
            m.args[1].integer(bytes) < kLanguageFirstNodeId ||
            (m.args[2].integer(bytes) != 0 && m.args[2].integer(bytes) != 1))
            return "Use an allocated language node, its default group, and addToHead/addToTail";
        for (std::size_t i = 4; i < end; i += 2)
            if ((m.args[i].type != 's' && m.args[i].type != 'i') ||
                !numeric(m.args[i + 1])) return "Expected finite numeric synth controls";
        return "";
    }
    if (a == "/g_new") {
        if (m.args.empty() || m.args.size() % 3) return "Expected group/action/target triples";
        for (std::size_t i = 0; i < m.args.size(); i += 3) {
            if (!node(i) || !integer(i + 1) || !integer(i + 2))
                return "Invalid group arguments";
            const auto id = m.args[i].integer(bytes);
            const auto action = m.args[i + 1].integer(bytes);
            const auto target = m.args[i + 2].integer(bytes);
            if ((action != 0 && action != 1) ||
                !(languageNode(target) || (id == 1 && target == 0)))
                return "Group operation would affect FoxDot's private nodes";
        }
        return "";
    }
    if (a == "/g_freeAll" || a == "/g_deepFree" || a == "/n_free") {
        if (m.args.empty()) return "Expected node IDs";
        for (auto& arg : m.args) {
            if (arg.type != 'i') return "Expected node IDs";
            const auto id = arg.integer(bytes);
            if ((a == "/g_freeAll" || a == "/g_deepFree") && id == 0)
                replaceOscInt(bytes, arg.offset, 1); // s.freeAll / CmdPeriod: SC group only.
            else if (!languageNode(id) || (a == "/n_free" && id == 1))
                return "Cannot free FoxDot's private nodes or the shared root";
        }
        return "";
    }
    if (a == "/n_set") {
        if (m.args.size() < 3 || !(m.args.size() & 1) || !node(0))
            return "Expected a language node and control/value pairs";
        for (std::size_t i = 1; i < m.args.size(); i += 2)
            if ((m.args[i].type != 's' && m.args[i].type != 'i') ||
                !numeric(m.args[i + 1])) return "Expected finite numeric controls";
        return "";
    }
    if (a == "/n_run")
        return m.args.size() == 2 && node(0) && integer(1) &&
            (m.args[1].integer(bytes) == 0 || m.args[1].integer(bytes) == 1)
            ? "" : "Expected a language node and run flag";
    return "Not supported by the bounded host-driven bridge; the VST engine was left running";
}

} // namespace dexter::sclang
