#pragma once

// WAV decoding and directory searches run only on the script worker, never in
// process(). No libsndfile, streaming, external server, or extra UGen is needed.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace PortableSamples {
constexpr std::size_t kMaxWaveFileBytes = 64u * 1024u * 1024u;
constexpr std::size_t kCacheBudgetBytes = 128u * 1024u * 1024u;

struct Wave {
    std::vector<float> mono;
    int frames = 0;
    int sourceChannels = 0;
    double sampleRate = 0;
};

inline bool readWave(const std::filesystem::path& path, Wave& result, std::string& error,
                     std::size_t decodedBudget = kCacheBudgetBytes) {
    result = {};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { error = "Could not open WAV: " + path.u8string(); return false; }
    const auto length = file.tellg();
    if (length < 44 || static_cast<std::uint64_t>(length) > kMaxWaveFileBytes) {
        error = "WAV must be between 44 bytes and 64 MiB: " + path.u8string(); return false;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        error = "Could not read complete WAV: " + path.u8string(); return false;
    }
    const auto u16 = [&](std::size_t at) -> unsigned {
        return bytes[at] | (unsigned(bytes[at + 1]) << 8);
    };
    const auto u32 = [&](std::size_t at) -> std::uint32_t {
        return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
               (std::uint32_t(bytes[at + 2]) << 16) | (std::uint32_t(bytes[at + 3]) << 24);
    };
    auto fail = [&](const char* reason) {
        error = std::string(reason) + ": " + path.u8string(); return false;
    };
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        return fail("Expected a little-endian RIFF/WAVE file");
    const std::uint64_t riffEnd = std::uint64_t(u32(4)) + 8;
    if (riffEnd > bytes.size() || riffEnd < 12) return fail("Truncated RIFF/WAVE");
    unsigned format = 0, channels = 0, bits = 0, alignment = 0, rate = 0;
    std::size_t dataAt = 0, dataSize = 0;
    bool haveFormat = false;
    for (std::uint64_t at = 12; at + 8 <= riffEnd;) {
        const std::size_t start = static_cast<std::size_t>(at);
        const std::uint64_t count = u32(start + 4);
        if (at + 8 + count > riffEnd) return fail("Truncated WAV chunk");
        if (!std::memcmp(bytes.data() + start, "fmt ", 4)) {
            if (count < 16 || haveFormat) return fail("Invalid WAV format chunk");
            haveFormat = true;
            format = u16(start + 8); channels = u16(start + 10);
            rate = u32(start + 12); alignment = u16(start + 20); bits = u16(start + 22);
            if (format == 0xfffe) {
                // WAVE_FORMAT_EXTENSIBLE: accept PCM/IEEE float GUIDs only.
                static constexpr unsigned char guidTail[14] =
                    {0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
                if (count < 40 || u16(start + 24) < 22 || u16(start + 26) != bits ||
                    std::memcmp(bytes.data() + start + 34, guidTail, 14))
                    return fail("Unsupported extensible WAV format");
                format = u16(start + 32);
            }
        } else if (!std::memcmp(bytes.data() + start, "data", 4)) {
            if (dataAt) return fail("Multiple WAV data chunks are not supported");
            dataAt = start + 8; dataSize = static_cast<std::size_t>(count);
        }
        at += 8 + count + (count & 1);
        if (at > riffEnd) return fail("Missing WAV chunk padding");
    }
    if (!haveFormat || !dataAt || !dataSize) return fail("WAV has no format or audio data");
    if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000)
        return fail("WAV must be mono/stereo at 8000-192000 Hz");
    if (!((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) ||
          (format == 3 && bits == 32)))
        return fail("Supported WAV encodings are PCM 8/16/24/32 or IEEE float32");
    const unsigned width = bits / 8;
    if (alignment != channels * width || dataSize % alignment)
        return fail("Invalid WAV frame alignment");
    const auto frames = dataSize / alignment;
    if (frames + 4 > decodedBudget / sizeof(float))
        return fail("Decoded WAV exceeds available sample-cache memory");
    result.mono.assign(frames + 4, 0.0f); // interpolation guards, not audio frames
    for (std::size_t frame = 0; frame < frames; ++frame) {
        double sum = 0;
        for (unsigned channel = 0; channel < channels; ++channel) {
            const auto at = dataAt + frame * alignment + channel * width;
            double value;
            if (format == 3) {
                const auto raw = u32(at); float sample;
                std::memcpy(&sample, &raw, sizeof sample); value = sample;
                if (!std::isfinite(value)) return fail("WAV contains non-finite audio");
                if (std::abs(value) > 16.0) return fail("Float WAV exceeds the safe sample-amplitude range");
            } else if (bits == 8) {
                value = (int(bytes[at]) - 128) / 128.0;
            } else {
                std::uint32_t raw = 0;
                for (unsigned byte = 0; byte < width; ++byte)
                    raw |= std::uint32_t(bytes[at + byte]) << (8 * byte);
                std::int64_t signedValue = raw;
                if (raw & (std::uint32_t(1) << (bits - 1))) signedValue -= std::int64_t(1) << bits;
                value = double(signedValue) / double(std::uint64_t(1) << (bits - 1));
            }
            sum += value;
        }
        // First batch uses one private mono bus per voice; stereo files are
        // deliberately downmixed, then positioned using the player's pan.
        result.mono[frame] = static_cast<float>(sum / channels);
    }
    result.frames = static_cast<int>(frames);
    result.sourceChannels = static_cast<int>(channels);
    result.sampleRate = rate;
    error.clear();
    return true;
}

inline std::filesystem::path symbolDirectory(const std::filesystem::path& root, int symbol) {
    if (symbol >= 'a' && symbol <= 'z')
        return root / std::string(1, static_cast<char>(symbol)) / "lower";
    if (symbol >= 'A' && symbol <= 'Z')
        return root / std::string(1, static_cast<char>(symbol + ('a' - 'A'))) / "upper";
    const char* name = nullptr;
    switch (symbol) {
    case '&': name = "ampersand"; break; case '*': name = "asterix"; break;
    case '@': name = "at"; break; case '^': name = "caret"; break;
    case ':': name = "colon"; break; case '$': name = "dollar"; break;
    case '=': name = "equals"; break; case '!': name = "exclamation"; break;
    case '/': name = "forwardslash"; break; case '#': name = "hash"; break;
    case '-': name = "hyphen"; break; case '%': name = "percent"; break;
    case '+': name = "plus"; break; case '?': name = "question"; break;
    case '~': name = "tilde"; break; case '\\': name = "backslash"; break;
    case '1': name = "1"; break; case '2': name = "2"; break;
    case '3': name = "3"; break; case '4': name = "4"; break;
    default: return {};
    }
    return root / "_" / name;
}

inline bool resolveWave(const std::filesystem::path& root, int symbol, int selection,
                        std::filesystem::path& result, std::string& error) {
    auto directory = symbolDirectory(root, symbol);
    if (directory.empty()) { error = "Unsupported sample symbol."; return false; }
    std::error_code ec;
    // Also accept the simpler Samples/x layout for alphabetic symbols.
    if (!std::filesystem::is_directory(directory, ec) &&
        ((symbol >= 'a' && symbol <= 'z') || (symbol >= 'A' && symbol <= 'Z')))
        directory = root / std::string(1, static_cast<char>(symbol));
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, ec);
    if (ec) { error = "Could not locate Portable/Samples."; return false; }
    const auto canonicalDirectory = std::filesystem::weakly_canonical(directory, ec);
    if (ec || std::mismatch(canonicalRoot.begin(), canonicalRoot.end(),
                           canonicalDirectory.begin(), canonicalDirectory.end()).first != canonicalRoot.end()) {
        error = "Sample folder must stay inside Portable/Samples."; return false;
    }
    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->is_symlink(ec)) continue;
        auto extension = it->path().extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".wav" || extension == ".wave") candidates.push_back(it->path());
    }
    if (ec || candidates.empty()) {
        error = "No WAV samples for '" + std::string(1, static_cast<char>(symbol)) +
                "'. Put WAV files in " + directory.u8string();
        return false;
    }
    std::sort(candidates.begin(), candidates.end());
    const auto count = static_cast<std::int64_t>(candidates.size());
    const auto index = (static_cast<std::int64_t>(selection) % count + count) % count;
    result = std::filesystem::weakly_canonical(candidates[static_cast<std::size_t>(index)], ec);
    if (ec) { error = "Could not resolve sample file."; return false; }
    return true;
}
} // namespace PortableSamples
