#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace dexter::sclang {

inline constexpr std::size_t kMaxOscStringBytes = 4096;

inline void appendOscString(std::vector<char>& packet, std::string_view value) {
    for (const char byte : value)
        packet.push_back(byte);
    packet.push_back('\0');
    while ((packet.size() & 3u) != 0)
        packet.push_back('\0');
}

inline std::vector<char> makeOscNoArgs(std::string_view address) {
    if (address.empty() || address.front() != '/' ||
        address.size() >= kMaxOscStringBytes)
        return {};

    std::vector<char> packet;
    packet.reserve(address.size() + 12);
    appendOscString(packet, address);
    appendOscString(packet, ",");
    return packet;
}

inline std::vector<char> makeOscString(std::string_view address,
                                       std::string_view value) {
    if (address.empty() || address.front() != '/' ||
        address.size() >= kMaxOscStringBytes ||
        value.size() >= kMaxOscStringBytes)
        return {};

    std::vector<char> packet;
    packet.reserve(address.size() + value.size() + 16);
    appendOscString(packet, address);
    appendOscString(packet, ",s");
    appendOscString(packet, value);
    return packet;
}

} // namespace dexter::sclang
