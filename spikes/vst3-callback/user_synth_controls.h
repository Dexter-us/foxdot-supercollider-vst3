#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dexter {

inline constexpr std::size_t kMaxPortableUserSynthControls = 7;
inline constexpr std::size_t kMaxPortableUserSynthControlFileBytes = 8192;

struct PortableUserSynthControlSpec {
    std::string name;
    int controlIndex = -1;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float defaultValue = 0.0f;
    bool logarithmic = false;

    float defaultNormalized() const noexcept {
        return normalize(defaultValue);
    }

    float map(float normalized) const noexcept {
        if (!std::isfinite(normalized))
            normalized = 0.0f;
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        if (logarithmic) {
            const double low = minimum;
            const double high = maximum;
            return static_cast<float>(
                low * std::pow(high / low, static_cast<double>(normalized)));
        }
        return minimum + normalized * (maximum - minimum);
    }

    float normalize(float value) const noexcept {
        if (!std::isfinite(value))
            return 0.0f;
        value = std::clamp(value, minimum, maximum);
        if (logarithmic) {
            const double low = minimum;
            const double high = maximum;
            return static_cast<float>(std::log(value / low) /
                                     std::log(high / low));
        }
        return (value - minimum) / (maximum - minimum);
    }
};

inline bool isPortableUserSynthControlName(std::string_view name) noexcept {
    if (name.empty() || name.size() > 31 ||
        !((name.front() >= 'A' && name.front() <= 'Z') ||
          (name.front() >= 'a' && name.front() <= 'z') ||
          name.front() == '_'))
        return false;
    for (const char ch : name) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_'))
            return false;
    }
    return true;
}

inline bool isReservedPortableUserSynthControl(
    std::string_view name) {
    static constexpr std::array<std::string_view, 10> reserved{{
        "out", "inbus", "freq", "amp", "pan", "gate", "atk", "rel",
        "sus", "sustain"}};
    std::string lowercase(name);
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(
                           ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
                   });
    if (lowercase.rfind("fx_", 0) == 0)
        return true;
    return std::find(reserved.begin(), reserved.end(), lowercase) !=
           reserved.end();
}

inline bool parsePortableUserSynthControlFloat(const std::string& text,
                                               float& value) {
    try {
        std::size_t used = 0;
        value = std::stof(text, &used);
        return used == text.size() && std::isfinite(value);
    } catch (...) {
        return false;
    }
}

// Optional sibling file: <SynthDefName>.controls
// Each non-comment line is: name minimum maximum default [linear|log]
// Only explicitly ranged, non-reserved parameters are exposed as live sliders.
inline bool parsePortableUserSynthControls(
    std::string_view text,
    std::vector<PortableUserSynthControlSpec>& controls,
    std::string& error) {
    controls.clear();
    error.clear();
    const auto reject = [&controls, &error](const std::string& message) {
        controls.clear();
        error = message;
        return false;
    };
    if (text.size() > kMaxPortableUserSynthControlFileBytes) {
        return reject("control sidecar exceeds the 8192-byte safety limit");
    }
    if (std::any_of(text.begin(), text.end(), [](unsigned char value) {
            return value == 0 ||
                   (value < 0x20 && value != '\n' && value != '\r' &&
                    value != '\t');
        })) {
        return reject("control sidecar contains a non-text control byte");
    }

    std::istringstream lines{std::string(text)};
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(lines, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos)
            line.resize(comment);
        std::istringstream fields(line);
        std::string name;
        if (!(fields >> name))
            continue;

        std::string minimumText;
        std::string maximumText;
        std::string defaultText;
        std::string scaleText;
        std::string extra;
        if (!(fields >> minimumText >> maximumText >> defaultText)) {
            return reject("line " + std::to_string(lineNumber) +
                          " must contain name, minimum, maximum, and default");
        }
        fields >> scaleText;
        if (fields >> extra) {
            return reject("line " + std::to_string(lineNumber) +
                          " has too many fields");
        }
        if (!isPortableUserSynthControlName(name) ||
            isReservedPortableUserSynthControl(name)) {
            return reject("line " + std::to_string(lineNumber) +
                          " uses an invalid or engine-reserved control name");
        }
        if (std::any_of(controls.begin(), controls.end(),
                        [&](const auto& control) {
                            return control.name == name;
                        })) {
            return reject("line " + std::to_string(lineNumber) +
                          " repeats a control name");
        }
        if (controls.size() >= kMaxPortableUserSynthControls) {
            return reject("control sidecar exceeds the seven-slider limit");
        }

        PortableUserSynthControlSpec control;
        control.name = std::move(name);
        if (!parsePortableUserSynthControlFloat(minimumText,
                                                control.minimum) ||
            !parsePortableUserSynthControlFloat(maximumText,
                                                control.maximum) ||
            !parsePortableUserSynthControlFloat(defaultText,
                                                control.defaultValue) ||
            !(control.minimum < control.maximum) ||
            control.defaultValue < control.minimum ||
            control.defaultValue > control.maximum) {
            return reject(
                "line " + std::to_string(lineNumber) +
                " has non-finite or inconsistent control bounds/default");
        }
        if (!scaleText.empty() && scaleText != "linear" &&
            scaleText != "log") {
            return reject("line " + std::to_string(lineNumber) +
                          " scale must be linear or log");
        }
        control.logarithmic = scaleText == "log";
        if (control.logarithmic &&
            (control.minimum <= 0.0f || control.maximum <= control.minimum)) {
            return reject("line " + std::to_string(lineNumber) +
                          " logarithmic bounds must be positive and increasing");
        }
        controls.push_back(std::move(control));
    }
    return true;
}

}  // namespace dexter
