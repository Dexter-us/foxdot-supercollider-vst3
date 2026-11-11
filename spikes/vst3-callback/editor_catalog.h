#pragma once

#include "player_line_edit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace dexter::editor {

enum class CatalogKind {
    Instrument,
    SamplePlayer,
    UserSynth,
    NativeControls,
    ExternalEffect
};

struct CatalogArgument {
    std::string name;
    std::string value;
};

struct CatalogEntry {
    CatalogKind kind = CatalogKind::Instrument;
    std::string name;
    std::string label;
    std::string about;
    std::vector<CatalogArgument> arguments;
    bool available = true;
    std::string category;
    std::string subcategory;

    CatalogEntry() = default;
    CatalogEntry(CatalogKind kindValue, std::string nameValue,
                 std::string labelValue, std::string aboutValue,
                 std::vector<CatalogArgument> argumentsValue = {},
                 bool availableValue = true,
                  std::string categoryValue = {},
                  std::string subcategoryValue = {})
        : kind(kindValue), name(std::move(nameValue)),
          label(std::move(labelValue)), about(std::move(aboutValue)),
          arguments(std::move(argumentsValue)), available(availableValue),
          category(std::move(categoryValue)),
          subcategory(std::move(subcategoryValue)) {}
};

inline std::string catalogLower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return result;
}

inline std::pair<std::string, std::string> portableSynthGrouping(
    std::string_view name) {
    // Put precompiled voices beside comparable built-ins, with instrument-level
    // subcategories instead of a generic "Precompiled ..." bucket.
    if (name == "PortedAnalogBassDrum")
        return {"Drums & Percussion", "Kick"};
    if (name == "PortedAnalogSnareDrum")
        return {"Drums & Percussion", "Snare"};
    if (name == "PortedStringVoice")
        return {"Plucked & Stringed", "String voices"};
    if (name == "PortedRongs" || name == "PortedPhasorModal" ||
        name == "PortedResonator")
        return {"Keys & Mallets", "Bells and resonators"};
    if (name == "PortedAnalogFoldOsc" || name == "PortedBLOsc" ||
        name == "PortedNeoVarSawOsc" || name == "PortedVosimOsc" ||
        name == "PortedZOsc")
        return {"Waveforms", "Ported oscillators"};
    if (name == "CustomPluck")
        return {"Plucked & Stringed", "Custom plucked voices"};
    if (name == "CustomTone")
        return {"Waveforms", "Custom oscillator voices"};
    const auto lowered = catalogLower(name);
    if (lowered.rfind("jazzkit", 0) == 0)
        return {"Drums & Percussion", "Jazz Kit"};
    const auto has = [&lowered](std::string_view token) {
        return lowered.find(token) != std::string::npos;
    };
    if (has("kick") || has("bassdrum"))
        return {"Drums & Percussion", "Kick"};
    if (has("snare") || has("sidestick") || has("rimshot"))
        return {"Drums & Percussion", "Snare"};
    if (has("hihat") || has("hi-hat") || has("hat"))
        return {"Drums & Percussion", "Hi-hat"};
    if (has("tom"))
        return {"Drums & Percussion", "Toms"};
    if (has("cymbal") || has("crash") || has("ride"))
        return {"Drums & Percussion", "Cymbals"};
    if (has("clap"))
        return {"Drums & Percussion", "Claps"};
    if (has("shaker") || has("maraca"))
        return {"Drums & Percussion", "Shakers"};
    if (has("bongo") || has("conga") || has("woodblock") ||
        has("clave") || has("cowbell") || has("percussion") ||
        has("perc") || has("drum"))
        return {"Drums & Percussion", "Hand percussion"};
    if (has("bass")) {
        if (has("acid"))
            return {"Bass", "Acid bass"};
        if (has("sub"))
            return {"Bass", "Sub bass"};
        if (has("upright") || has("doublebass") || has("dbass"))
            return {"Bass", "Upright bass"};
        if (has("electric") || has("jbass"))
            return {"Bass", "Electric bass"};
        if (has("saw"))
            return {"Bass", "Saw bass"};
        return {"Bass", "Synth bass"};
    }
    if (has("bell") || has("gong") || has("mallet") ||
        has("piano") || has("keys") || has("marimba"))
        return {"Keys & Mallets",
                (has("bell") || has("gong") || has("glass"))
                    ? "Bells and resonators"
                    : (has("marimba") || has("mallet")) ? "Mallets"
                                                         : "Keyboard"};
    if (has("pluck") || has("harp") || has("guitar") ||
        has("string") || has("viola"))
        return {"Plucked & Stringed",
                (has("guitar") || has("viola") || has("violin") ||
                 has("string"))
                    ? "String voices"
                    : "Plucked voices"};
    if (has("pad") || has("ambient"))
        return {"Pads & Atmospheres", "Pads and sustained textures"};
    if (has("noise") || has("texture"))
        return {"Textures & Noise",
                has("noise") ? "Noise" : "Sound-design textures"};
    if (has("sine") || has("saw") || has("pulse") || has("wave") ||
        has("osc") || has("triangle"))
        return {"Waveforms", "Custom oscillators"};
    return {"User Synths", "Custom SynthDefs"};
}

inline std::string portableSynthDisplayLabel(std::string_view name) {
    static constexpr std::array<
        std::pair<std::string_view, std::string_view>, 11> portedLabels{{
        {"PortedAnalogBassDrum", "Analog bass drum"},
        {"PortedAnalogSnareDrum", "Analog snare drum"},
        {"PortedAnalogFoldOsc", "Analog wavefolder oscillator"},
        {"PortedBLOsc", "Band-limited oscillator"},
        {"PortedNeoVarSawOsc", "Variable-slope saw"},
        {"PortedStringVoice", "Karplus–Strong string"},
        {"PortedVosimOsc", "VOSIM oscillator"},
        {"PortedZOsc", "Z oscillator"},
        {"PortedRongs", "Rongs modal resonator"},
        {"PortedPhasorModal", "Phasor-modal voice"},
        {"PortedResonator", "Resonant body voice"}
    }};
    for (const auto& item : portedLabels) {
        if (name == item.first)
            return std::string(item.second);
    }
    static constexpr std::array<
        std::pair<std::string_view, std::string_view>, 10> jazzLabels{{
        {"JazzKitKick", "Kick"},
        {"JazzKitBrushKick", "Brush kick"},
        {"JazzKitBrushTap", "Brush snare tap"},
        {"JazzKitBrushSweep", "Brush sweep"},
        {"JazzKitClosedHat", "Closed brush hat"},
        {"JazzKitOpenHat", "Open brush hat"},
        {"JazzKitRide", "Ride cymbal"},
        {"JazzKitCrossStick", "Cross-stick"},
        {"JazzKitRackTom", "Rack tom"},
        {"JazzKitFloorTom", "Floor tom"}
    }};
    for (const auto& item : jazzLabels) {
        if (name == item.first)
            return std::string(item.second);
    }
    return std::string(name) + " [Portable SynthDef]";
}

inline std::string instrumentCategory(const CatalogEntry& entry) {
    if (entry.kind == CatalogKind::SamplePlayer)
        return "Drums & Percussion";
    if (entry.kind == CatalogKind::UserSynth)
        return portableSynthGrouping(entry.name).first;
    static const std::vector<std::string> waveforms{
        "pulse", "saw", "sine", "triangle", "varsaw"};
    static const std::vector<std::string> bass{
        "bass", "dbass", "jbass", "sawbass"};
    static const std::vector<std::string> keys{
        "bell", "glass", "gong", "keys", "klank", "marimba"};
    static const std::vector<std::string> plucked{
        "donk", "karp", "nylon", "pluck", "sitar", "squish", "twang",
        "viola"};
    static const std::vector<std::string> pads{
        "ambi", "pads", "sinepad", "space", "swell"};
    static const std::vector<std::string> texture{
        "ambi", "creep", "dirt", "fuzz", "noise", "orient", "scratch",
        "scatter", "spark"};
    if (std::find(waveforms.begin(), waveforms.end(), entry.name) !=
        waveforms.end())
        return "Waveforms";
    if (std::find(bass.begin(), bass.end(), entry.name) != bass.end())
        return "Bass";
    if (std::find(keys.begin(), keys.end(), entry.name) != keys.end())
        return "Keys & Mallets";
    if (std::find(plucked.begin(), plucked.end(), entry.name) != plucked.end())
        return "Plucked & Stringed";
    if (std::find(pads.begin(), pads.end(), entry.name) != pads.end())
        return "Pads & Atmospheres";
    if (std::find(texture.begin(), texture.end(), entry.name) != texture.end())
        return "Textures & Noise";
    return "Leads & Synth Voices";
}

inline std::string instrumentSubcategory(const CatalogEntry& entry) {
    const auto& name = entry.name;
    if (entry.kind == CatalogKind::SamplePlayer)
        return "WAV sample player";
    if (entry.kind == CatalogKind::UserSynth)
        return portableSynthGrouping(name).second;
    if (entry.category == "Drums & Percussion")
        return "Sample player";
    if (entry.category == "Bass") {
        if (name == "dbass")
            return "Upright bass";
        if (name == "jbass")
            return "Electric bass";
        if (name == "sawbass")
            return "Saw bass";
        return "Synth bass";
    }
    if (entry.category == "Keys & Mallets") {
        if (name == "bell" || name == "gong" || name == "glass")
            return "Bells and resonators";
        if (name == "marimba")
            return "Mallets";
        return "Keyboard";
    }
    if (entry.category == "Plucked & Stringed") {
        if (name == "nylon" || name == "viola" || name == "twang")
            return "String voices";
        return "Plucked voices";
    }
    if (entry.category == "Pads & Atmospheres")
        return "Pads and sustained textures";
    if (entry.category == "Textures & Noise") {
        if (name == "noise")
            return "Noise";
        if (name == "scratch" || name == "scatter")
            return "Rhythmic textures";
        return "Sound-design textures";
    }
    if (entry.category == "Waveforms") {
        if (name == "varsaw")
            return "Variable-width oscillator";
        return "Core oscillators";
    }
    if (name == "arpy")
        return "Arpeggiated";
    if (name == "lazer" || name == "rave" || name == "prophet")
        return "Electronic leads";
    return "Lead and character voices";
}

inline std::string effectCategory(const CatalogEntry& entry) {
    const auto& name = entry.name;
    if (name == "DexterGranularFX")
        return "Granular";
    if (name == "PortedResonatorFX")
        return "Resonators";
    if (name == "PortedFverbFX")
        return "Reverb";
    if (name == "PortedAnalogChewFX" ||
        name == "PortedAnalogDegradeFX" ||
        name == "PortedAnalogLossFX" ||
        name == "PortedAnalogTapeFX")
        return "Tape and lo-fi";
    if (name == "PortedAnalogPhaserFX")
        return "Modulation and rhythm";
    if (name == "PortedDCompressorFX")
        return "Dynamics";
    if (name == "PortedAnalogVintageDistortionFX" ||
        name == "PortedLockhartWavefolderFX")
        return "Distortion and reduction";
    if (name == "Echo" || name == "AirwindowsEchojam" ||
        name == "AirwindowsTapeDelay2" || name == "LimutPingPong")
        return "Delay";
    if (name == "Freeverb" || name == "Freeverb2" || name == "GVerb" ||
        name == "Greyhole" || name == "JPverb" ||
        name == "LimutShimmer" || name == "LimutGatedVerb")
        return "Reverb";
    if (name == "Chorus" || name == "Flanger" || name == "Phaser" ||
        name == "SCLOrkPhazerFX" || name == "Tremolo" ||
        name == "Chop" || name == "Spin")
        return "Modulation and rhythm";
    if (name == "Low-pass" || name == "High-pass" ||
        name == "Band-pass" || name == "LFO filter")
        return "Filters";
    if (name == "Vibrato" || name == "Slide" || name == "Bend" ||
        name == "PShift" || name == "Coarse" || name == "Formant" ||
        name == "LimutShifter")
        return "Pitch and formant";
    if (name == "Bits / Crush" || name == "Shape" || name == "Drive" ||
        name == "SCLOrkDecimatorFX" ||
        name == "SCLOrkCrossoverDistortionFX")
        return "Distortion and reduction";
    if (name == "LimutDuck")
        return "Duck";
    if (name == "LimutDisperser")
        return "Phase";
    if (name == "LimutFuzz")
        return "Distortion and reduction";
    if (name == "LimutCompressor")
        return "Dynamics";
    if (name == "LimutVocoder")
        return "Vocoder";
    if (name == "Cut" || name == "Blur" || name == "FMod")
        return "Player controls";
    return "Other effects";
}

inline std::string effectSubcategory(const CatalogEntry& entry) {
    const auto& name = entry.name;
    if (name == "DexterGranularFX")
        return "Cloud / Slicer / Freeze / Live";
    if (name == "PortedResonatorFX")
        return "Resonant body";
    if (name == "PortedFverbFX")
        return "Fverb / Dattorro";
    if (name == "PortedAnalogTapeFX")
        return "Tape saturation";
    if (name == "PortedAnalogChewFX")
        return "Tape chew";
    if (name == "PortedAnalogDegradeFX")
        return "Tape wear";
    if (name == "PortedAnalogLossFX")
        return "Tape head loss";
    if (name == "PortedAnalogPhaserFX")
        return "Analog phaser";
    if (name == "PortedAnalogVintageDistortionFX")
        return "Vintage EQ and distortion";
    if (name == "PortedLockhartWavefolderFX")
        return "Wavefolding";
    if (name == "PortedDCompressorFX")
        return "Self-keyed compression";
    if (entry.category == "Delay") {
        if (name == "AirwindowsTapeDelay2")
            return "Tape";
        if (name == "LimutPingPong")
            return "Ping-pong";
        if (name == "AirwindowsEchojam")
            return "Multi-tap echo";
        return "Echo";
    }
    if (entry.category == "Reverb") {
        if (name == "LimutShimmer")
            return "Pitch-shift shimmer";
        if (name == "LimutGatedVerb")
            return "Gated";
        if (name == "Greyhole" || name == "JPverb")
            return "Space";
        return "Room";
    }
    if (entry.category == "Modulation and rhythm") {
        if (name == "Chop" || name == "Spin")
            return "Rhythmic";
        return name;
    }
    if (entry.category == "Filters")
        return name;
    if (entry.category == "Pitch and formant") {
        if (name == "Vibrato")
            return "Vibrato";
        if (name == "Slide" || name == "Bend")
            return "Glide and bend";
        if (name == "Formant")
            return "Formant";
        return "Pitch shift";
    }
    if (entry.category == "Distortion and reduction") {
        if (name == "Bits / Crush" || name == "SCLOrkDecimatorFX")
            return "Bit/sample reduction";
        return "Waveshaping and drive";
    }
    if (entry.category == "Duck")
        return "Envelope ducking";
    if (entry.category == "Dynamics")
        return "Compression";
    if (entry.category == "Phase")
        return "All-pass / dispersion";
    if (entry.category == "Vocoder")
        return "Vocoder";
    if (entry.category == "Player controls")
        return "Per-note controls";
    return "Other";
}

inline std::pair<std::string, std::string> sampleCategoryForFilename(
    std::string_view filename) {
    const auto lowered = catalogLower(filename);
    const auto contains = [&](std::string_view text) {
        return lowered.find(text) != std::string::npos;
    };
    const auto has = [&](std::string_view token) {
        const auto found = lowered.find(token);
        if (found == std::string::npos)
            return false;
        const auto isWord = [](unsigned char character) {
            return std::isalnum(character) != 0;
        };
        const bool leftBoundary =
            found == 0 || !isWord(static_cast<unsigned char>(lowered[found - 1]));
        const auto end = found + token.size();
        const bool rightBoundary =
            end == lowered.size() ||
            !isWord(static_cast<unsigned char>(lowered[end]));
        return leftBoundary && rightBoundary;
    };
    if (contains("kickdrum") || contains("bassdrum") ||
        has("kick") || has("bd"))
        return {"Drums & Percussion", "Kick"};
    if (contains("snaredrum") || contains("sidestick") ||
        has("snare") || has("sd"))
        return {"Drums & Percussion", "Snare"};
    if (contains("hihat") || contains("hi-hat") ||
        has("hat") || has("hh"))
        return {"Drums & Percussion", "Hi-hat"};
    if (has("tom") || has("toms"))
        return {"Drums & Percussion", "Toms"};
    if (has("cymbal") || has("ride") || has("crash"))
        return {"Drums & Percussion", "Cymbals"};
    if (has("clap"))
        return {"Drums & Percussion", "Claps"};
    if (has("shaker") || has("maraca"))
        return {"Drums & Percussion", "Shakers"};
    if (has("cowbell") || has("bongo") || has("conga") ||
        has("woodblock") || has("clave") || has("rim") ||
        has("percussion") || has("perc"))
        return {"Drums & Percussion", "Hand percussion"};
    return {"Other Samples", "Tonal and FX"};
}

struct PlayerEffectControlSpec {
    std::string name;
    float minimum = 0.0f;
    float maximum = 1.0f;
    int controlIndex = -1;
};

inline std::vector<PlayerEffectControlSpec> playerEffectControlSpecs(
    std::string_view effectName) {
    if (effectName == "AirwindowsTapeDelay2")
        return {{"fx_mix", 0, 1, 5}, {"fx_vib", 0, 1, 0},
                {"fx_depth", 0, 1, 1}, {"fx_lforate", 0, 1, 2},
                {"fx_lfowidth", 0, 1, 3}, {"fx_rq", 0, 1, 4},
                {"fx_blur", 0, 1, 6}};
    if (effectName == "AirwindowsEchojam")
        return {{"fx_mix", 0, 1, 5}, {"fx_vib", 0, 1, 0},
                {"fx_depth", 0, 1, 1}, {"fx_lforate", 0, 1, 2},
                {"fx_blur", 0, 1, 6}};
    if (effectName == "JPverb")
        return {{"fx_mix", 0, 1}, {"fx_t60", 0.1f, 60},
                {"fx_damp", 0, 0.999f}, {"fx_size", 0.5f, 3},
                {"fx_earlydiff", 0, 0.99f}, {"fx_moddepth", 0, 1},
                {"fx_modfreq", 0, 10}};
    if (effectName == "Greyhole")
        return {{"fx_mix", 0, 1}, {"fx_delay", 0.001f, 1.45f},
                {"fx_damp", 0, 0.99f}, {"fx_size", 0.5f, 3},
                {"fx_diff", 0, 0.99f}, {"fx_feedback", 0, 0.98f},
                {"fx_moddepth", 0, 1}};
    if (effectName == "Chorus")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 5},
                {"fx_depth", 0, 1}, {"fx_delay", 0.005f, 0.04f},
                {"fx_spread", 0, 1}, {"fx_feedback", 0, 0.75f},
                {"fx_tone", 200, 16000}};
    if (effectName == "Flanger")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 4},
                {"fx_depth", 0, 1}, {"fx_delay", 0.0005f, 0.015f},
                {"fx_spread", 0, 1}, {"fx_feedback", 0, 0.95f},
                {"fx_tone", 200, 16000}};
    if (effectName == "Phaser")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 8},
                {"fx_depth", 0, 1}, {"fx_center", 150, 1800},
                {"fx_spread", 0, 1}, {"fx_feedback", 0, 0.85f},
                {"fx_tone", 200, 16000}};
    if (effectName == "SCLOrkPhazerFX")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 8},
                {"fx_depth", 0, 1}, {"fx_center", 150, 1800},
                {"fx_spread", 0, 1}, {"fx_q", 0.05f, 1},
                {"fx_tone", 200, 16000}};
    if (effectName == "SCLOrkDecimatorFX")
        return {{"fx_mix", 0, 1}, {"fx_rate", 80, 22000},
                {"fx_bits", 1, 16}, {"fx_drive", 0.5f, 8},
                {"fx_tone", 200, 16000}};
    if (effectName == "SCLOrkCrossoverDistortionFX")
        return {{"fx_mix", 0, 1}, {"fx_drive", 0.5f, 8},
                {"fx_shape", 0, 1}, {"fx_tone", 200, 16000}};
    if (effectName == "Tremolo")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 20},
                {"fx_depth", 0, 1}, {"fx_shape", 0, 1},
                {"fx_spread", 0, 1}, {"fx_phase", 0, 1},
                {"fx_tone", 200, 16000}};
    if (effectName == "LimutPingPong")
        return {{"fx_mix", 0, 1}, {"fx_time", 0.01f, 2},
                {"fx_feedback", 0, 0.95f}};
    if (effectName == "LimutDuck")
        return {{"fx_mix", 0, 1}, {"fx_period", 0.05f, 8},
                {"fx_depth", 0, 1}, {"fx_length", 0, 8},
                {"fx_attack", 0.001f, 8}};
    if (effectName == "LimutDisperser")
        return {{"fx_mix", 0, 1}, {"fx_frequency", 30, 18000},
                {"fx_q", 0.1f, 10}, {"fx_stages", 1, 24}};
    if (effectName == "LimutFuzz")
        return {{"fx_mix", 0, 1}, {"fx_drive", 0, 36},
                {"fx_tone", 200, 16000}};
    if (effectName == "LimutShifter")
        return {{"fx_mix", 0, 1}, {"fx_ratio", 0.5f, 2},
                {"fx_window", 0.02f, 0.5f}};
    if (effectName == "LimutShimmer")
        return {{"fx_mix", 0, 1}, {"fx_ratio", 0.5f, 2},
                {"fx_window", 0.02f, 0.5f}, {"fx_room", 0, 1},
                {"fx_damp", 0, 1}};
    if (effectName == "LimutGatedVerb")
        return {{"fx_mix", 0, 1}, {"fx_room", 0, 1},
                {"fx_damp", 0, 1}, {"fx_threshold", 0.001f, 0.25f},
                {"fx_hold", 0, 2}, {"fx_attack", 0.001f, 0.25f},
                {"fx_release", 0.01f, 1}};
    if (effectName == "LimutCompressor")
        return {{"fx_mix", 0, 1}, {"fx_threshold", -60, 0},
                {"fx_ratio", 0, 20}, {"fx_attack", 0, 1},
                {"fx_release", 0.001f, 3}, {"fx_knee", 0, 24}};
    if (effectName == "PortedResonatorFX")
        return {{"fx_mix", 0, 1}, {"fx_freq", 35, 6000},
                {"fx_structure", 0, 1}, {"fx_brightness", 0, 1},
                {"fx_damping", 0, 1}};
    if (effectName == "PortedFverbFX")
        return {{"fx_mix", 0, 1}, {"fx_predelay", 0, 10},
                {"fx_decay", 0, 100}, {"fx_damping", 10, 20000},
                {"fx_mod_depth", 0, 1}};
    if (effectName == "PortedAnalogTapeFX")
        return {{"fx_mix", 0, 1}, {"fx_bias", 0, 1},
                {"fx_saturation", 0, 1}, {"fx_drive", 0, 1}};
    if (effectName == "PortedAnalogChewFX")
        return {{"fx_mix", 0, 1}, {"fx_depth", 0, 1},
                {"fx_frequency", 0, 1}, {"fx_variance", 0, 1}};
    if (effectName == "PortedAnalogDegradeFX")
        return {{"fx_mix", 0, 1}, {"fx_depth", 0, 1},
                {"fx_amount", 0, 1}, {"fx_variance", 0, 1},
                {"fx_envelope", 0, 1}};
    if (effectName == "PortedAnalogLossFX")
        return {{"fx_mix", 0, 1}, {"fx_gap", 0, 1},
                {"fx_thick", 0, 1}, {"fx_space", 0, 1},
                {"fx_speed", 0, 1}};
    if (effectName == "PortedAnalogPhaserFX")
        return {{"fx_mix", 0, 1}, {"fx_rate", 0.05f, 8},
                {"fx_skew", -1, 1}, {"fx_feedback", 0, 0.95f},
                {"fx_modulation", 0, 1}, {"fx_stages", 1, 50}};
    if (effectName == "PortedAnalogVintageDistortionFX")
        return {{"fx_mix", 0, 1}, {"fx_drivegain", 0, 1},
                {"fx_bias", 0, 2.5f}, {"fx_lowgain", 0.0001f, 0.3f},
                {"fx_highgain", 0.001f, 0.3f},
                {"fx_shelvingfreq", 20, 20000}};
    if (effectName == "PortedLockhartWavefolderFX")
        return {{"fx_mix", 0, 1}, {"fx_bias", 0, 1},
                {"fx_pregain", 1, 20}};
    if (effectName == "PortedDCompressorFX")
        return {{"fx_mix", 0, 1}, {"fx_ratio", 1, 40},
                {"fx_threshold", -80, 0}, {"fx_attack", 0.001f, 10},
                {"fx_release", 0.001f, 10}, {"fx_makeup", 0, 24},
                {"fx_automakeup", 0, 1}};
    if (effectName == "DexterGranularFX")
        return {{"fx_mix", 0, 1}, {"fx_mode", 0, 3},
                {"fx_density", 1, 40}, {"fx_size", 0.01f, 0.25f},
                {"fx_pitch", 0.5f, 2}, {"fx_position", 0, 1},
                {"fx_freeze", 0, 1}};
    return {};
}

inline int playerEffectMixControlIndex(std::string_view effectName) noexcept {
    if (effectName == "AirwindowsTapeDelay2" ||
        effectName == "AirwindowsEchojam")
        return 5;
    if (effectName == "JPverb" || effectName == "Greyhole" ||
        effectName == "Chorus" || effectName == "Flanger" ||
        effectName == "Phaser" || effectName == "Tremolo" ||
        effectName == "SCLOrkPhazerFX" ||
        effectName == "SCLOrkDecimatorFX" ||
        effectName == "SCLOrkCrossoverDistortionFX" ||
        effectName == "LimutPingPong" || effectName == "LimutDuck" ||
        effectName == "LimutDisperser" || effectName == "LimutFuzz" ||
        effectName == "LimutShifter" || effectName == "LimutShimmer" ||
        effectName == "LimutGatedVerb" || effectName == "LimutCompressor" ||
        effectName == "PortedResonatorFX" || effectName == "PortedFverbFX" ||
        effectName == "PortedAnalogTapeFX" ||
        effectName == "PortedAnalogChewFX" ||
        effectName == "PortedAnalogDegradeFX" ||
        effectName == "PortedAnalogLossFX" ||
        effectName == "PortedAnalogPhaserFX" ||
        effectName == "PortedAnalogVintageDistortionFX" ||
        effectName == "PortedLockhartWavefolderFX" ||
        effectName == "PortedDCompressorFX" ||
        effectName == "DexterGranularFX")
        return 0;
    return -1;
}

inline int firstConfiguredEffectSlot(
    const std::array<int, 4>& effectIndices) {
    for (std::size_t slot = 0; slot < effectIndices.size(); ++slot)
        if (effectIndices[slot] >= 0)
            return static_cast<int>(slot);
    return -1;
}

inline std::string catalogInsertCall(const CatalogEntry& entry) {
    if (entry.kind == CatalogKind::Instrument)
        return entry.name + "(P[0, 2, 4, 7], dur=0.5, amp=0.25)";
    if (entry.kind == CatalogKind::SamplePlayer)
        return "play(\"x-o-\", dur=0.5, sample=0, amp=0.25)";
    if (entry.kind == CatalogKind::UserSynth)
        return "userSynth(\"" + entry.name +
               "\", P[0, 2, 4, 7], dur=0.5, amp=0.25)";
    return {};
}

inline bool applyCatalogEntryInPlace(std::string& source, std::size_t position,
                                     const CatalogEntry& entry,
                                     std::string& failure) {
    PlayerAssignment assignment;
    if (!entry.available) {
        failure = "This effect requires its optional SynthDef pack.";
        return false;
    }
    if (!findPlayerLineAt(source, position, assignment, true)) {
        failure = "Right-click a p1 through p8 player line first.";
        return false;
    }
    if (assignment.callOpen == assignment.callClose) {
        if (entry.kind == CatalogKind::Instrument ||
            entry.kind == CatalogKind::SamplePlayer ||
            entry.kind == CatalogKind::UserSynth) {
            if (!insertPlayerCall(source, assignment,
                                  " " + catalogInsertCall(entry))) {
                failure = "Could not create the player call.";
                return false;
            }
            return true;
        }
        const CatalogEntry defaultSynth{
            CatalogKind::Instrument, "sine", "sine",
            "Default local synth.", {}, true};
        if (!insertPlayerCall(source, assignment,
                              " " + catalogInsertCall(defaultSynth)) ||
            !findPlayerAssignmentAt(source, assignment.lineStart,
                                    assignment)) {
            failure = "Could not create the default player call.";
            return false;
        }
    }

    const auto synth = source.substr(
        assignment.synthStart, assignment.synthEnd - assignment.synthStart);
    if (entry.kind == CatalogKind::Instrument) {
        if (synth == "play") {
            failure = "Sample players stay play(...); choose a sample item instead.";
            return false;
        }
        if (synth == "userSynth") {
            failure = "Start from a blank player line to select a bundled synth.";
            return false;
        }
        return replacePlayerSynth(source, assignment, entry.name);
    }
    if (entry.kind == CatalogKind::SamplePlayer) {
        if (synth != "play") {
            failure = "Use the sample player on a blank pN >> line.";
            return false;
        }
        failure = "This player already uses play(). Choose its sample from "
                  "the Samples panel.";
        return false;
    }
    if (entry.kind == CatalogKind::UserSynth) {
        failure = "Use a blank pN >> line to insert this custom SynthDef.";
        return false;
    }

    if (entry.kind == CatalogKind::NativeControls) {
        for (const auto& argument : entry.arguments) {
            if (!setPlayerArgument(source, assignment, argument.name,
                                   argument.value)) {
                failure = "Could not add the selected native controls.";
                return false;
            }
            if (!findPlayerAssignmentAt(source, position, assignment)) {
                failure = "Could not re-read the edited player line.";
                return false;
            }
        }
        return true;
    }

    if (entry.kind == CatalogKind::ExternalEffect) {
        if (synth != "play" && synth != "userSynth" &&
            !isSimpleIdentifier(synth)) {
            failure = "This player call cannot accept an effect.";
            return false;
        }
        if (synth == "play" && entry.name == "CustomPluck") {
            failure = "A pitched SynthDef cannot replace a sample player.";
            return false;
        }
        std::size_t chainStart = 0;
        std::size_t chainEnd = 0;
        const bool hadChain = findArgumentValue(
            source, assignment, "effects", true, chainStart, chainEnd);
        if (hadChain) {
            if (!setPlayerListItem(source, assignment, "effects", entry.name)) {
                failure = "A player can have at most four effect inserts.";
                return false;
            }
        } else {
            if (!setPlayerEffect(source, assignment, entry.name))
                return false;
            if (!findPlayerAssignmentAt(source, position, assignment))
                return false;
            const bool hasMixControl =
                entry.name != "AirwindowsEchojam" &&
                entry.name != "AirwindowsTapeDelay2";
            if (hasMixControl &&
                !setPlayerEffectMix(source, assignment, "0.25")) {
                failure = "Effect added, but its initial mix could not be set.";
                return false;
            }
        }
        return true;
    }
    failure = "Unsupported catalog item.";
    return false;
}

inline bool applyCatalogEntry(std::string& source, std::size_t position,
                              const CatalogEntry& entry,
                              std::string& failure) {
    auto edited = source;
    if (!applyCatalogEntryInPlace(edited, position, entry, failure))
        return false;
    source.swap(edited);
    return true;
}

inline std::vector<CatalogEntry> makeInstrumentCatalog(
    std::vector<std::string> embedded,
    const std::vector<std::string>& external) {
    std::vector<CatalogEntry> result;
    for (auto& name : embedded) {
        if (name == "sample")
            continue;
        if (name == "square")
            name = "pulse";
        if (!isSimpleIdentifier(name) ||
            std::any_of(result.begin(), result.end(), [&](const auto& item) {
                return item.name == name;
            }))
            continue;
        result.push_back({CatalogKind::Instrument, name, name,
            "Built-in FoxDot instrument. Emits notes with the plugin's local "
            "synth graph; supports note, duration, octave, level, pan and "
            "the listed FoxDot controls. No SuperCollider server is started.",
            {}, true});
    }
    result.push_back({CatalogKind::SamplePlayer, "play", "play (sample)",
        "FoxDot sample player. Uses WAV files installed under Portable/Samples; "
        "a letter bank can be selected with sample= or per-hit selectors.",
        {}, true});
    static const std::vector<std::string> bundledEffects{
        "AirwindowsEchojam", "AirwindowsTapeDelay2", "Chorus", "Flanger",
        "Greyhole", "JPverb", "LimutDisperser", "LimutDuck",
        "LimutCompressor", "LimutFuzz", "LimutGatedVerb",
        "LimutPingPong", "LimutShifter", "LimutVocoder",
        "LimutShimmer", "Phaser", "SCLOrkPhazerFX",
        "SCLOrkDecimatorFX", "SCLOrkCrossoverDistortionFX", "Tremolo",
        "PortedResonatorFX", "PortedFverbFX", "PortedAnalogTapeFX",
        "PortedAnalogChewFX", "PortedAnalogDegradeFX",
        "PortedAnalogLossFX", "PortedAnalogPhaserFX",
        "PortedAnalogVintageDistortionFX", "PortedLockhartWavefolderFX",
        "PortedDCompressorFX", "DexterGranularFX"};
    for (const auto& name : external) {
        if (std::find(bundledEffects.begin(), bundledEffects.end(), name) !=
                bundledEffects.end() ||
            !isSimpleIdentifier(name) ||
            std::any_of(result.begin(), result.end(), [&](const auto& item) {
                return item.name == name;
            }))
            continue;
        result.push_back({CatalogKind::UserSynth, name,
            portableSynthDisplayLabel(name),
            "A precompiled user SynthDef loaded from Portable/SynthDefs/" +
                name +
                ".scsyndef. It does not load new UGen binaries; those require "
                "an installed compatible SCPlugins module and host restart.",
            {}, true});
    }
    for (auto& entry : result) {
        entry.category = instrumentCategory(entry);
        entry.subcategory = instrumentSubcategory(entry);
    }
    const auto less = [](const auto& left, const auto& right) {
        return std::make_tuple(left.category, left.subcategory,
                               catalogLower(left.label)) <
               std::make_tuple(right.category, right.subcategory,
                               catalogLower(right.label));
    };
    std::sort(result.begin(), result.end(), less);
    return result;
}

inline std::vector<CatalogEntry> makeEffectCatalog(
    const std::vector<std::string>& external) {
    using Arg = CatalogArgument;
    std::vector<CatalogEntry> result{
        {CatalogKind::NativeControls, "Echo", "Echo / delay",
         "FoxDot echo is 0..64 beats; delay is 0..8 beats; delaytime is "
         "seconds (0..4), decay is beats (0..64), "
         "echomix is wet level (0..1). This preset adds echo=0.35.",
         {Arg{"echo", "0.35"}}},
        {CatalogKind::NativeControls, "Freeverb", "Freeverb",
         "Bundled room reverb. reverb selects the algorithm; room and mix are "
         "0..1, damp is 0..1, revtime is 0.1..10 seconds, spread is 0..100. "
         "Preset: Freeverb, room=0.7, mix=0.2.",
         {Arg{"reverb", "\"freeverb\""}, Arg{"room", "0.7"},
          Arg{"mix", "0.2"}}},
        {CatalogKind::NativeControls, "Freeverb2", "Freeverb2",
         "Alternative bundled Freeverb response. Uses the same room, mix, "
         "damp, revtime and spread controls; room/mix/damp are 0..1, "
         "revtime is 0.1..10 seconds and spread is 0..100.",
         {Arg{"reverb", "\"freeverb2\""}, Arg{"room", "0.7"},
          Arg{"mix", "0.2"}}},
        {CatalogKind::NativeControls, "GVerb", "GVerb",
         "Bundled GVerb. Use reverb=\"gverb\"; room, mix, damp, revtime and "
         "spread are bounded room/mix/damp 0..1, revtime 0.1..10 seconds, "
         "spread 0..100.",
         {Arg{"reverb", "\"gverb\""}, Arg{"room", "0.7"},
          Arg{"mix", "0.2"}}},
        {CatalogKind::NativeControls, "Low-pass", "Filter: low-pass",
         "Low-pass cutoff uses lpf in Hz (0 disables, otherwise 20..20,000); "
         "lpr is resonance/Q (0.05..1).", {Arg{"lpf", "1800"},
                                             Arg{"lpr", "1.0"}}},
        {CatalogKind::NativeControls, "High-pass", "Filter: high-pass",
         "High-pass cutoff uses hpf in Hz (0 disables, otherwise 20..20,000); "
         "hpr is resonance/Q (0.05..1).", {Arg{"hpf", "250"},
                                             Arg{"hpr", "1.0"}}},
        {CatalogKind::NativeControls, "Band-pass", "Filter: band-pass",
         "Band-pass center uses bpf in Hz (0 disables, otherwise 20..20,000); "
         "bpr is resonance/Q (0.05..1).",
         {Arg{"bpf", "1200"}, Arg{"bpr", "0.7"}}},
        {CatalogKind::NativeControls, "Vibrato", "Vibrato",
         "Native pitch vibrato. vib is rate (0..100); depth and vibdepth are "
         "pitch-depth controls bounded 0..10.",
         {Arg{"vib", "5"}, Arg{"depth", "0.25"}, Arg{"vibdepth", "0.5"}}},
        {CatalogKind::NativeControls, "Slide", "Pitch slide",
         "FoxDot slide glides to the target pitch (−1..4); slidefrom changes "
         "the starting pitch (−1..4); slidedelay delays the glide (0..1 of "
         "the event).",
         {Arg{"slide", "0.5"}, Arg{"slidedelay", "0.1"}}},
        {CatalogKind::NativeControls, "Bend", "Pitch bend",
         "FoxDot bend moves pitch during a note (−1..4); benddelay delays its "
         "start (0..1 of the event).",
         {Arg{"bend", "1.0"}, Arg{"benddelay", "0.1"}}},
        {CatalogKind::NativeControls, "PShift", "Pitch shift",
         "Per-player, per-note pitch shift in semitones, bounded −24..24. "
         "It appears in that player's sliders only when pshift is enabled.",
         {Arg{"pshift", "7"}}},
        {CatalogKind::NativeControls, "Bits / Crush", "Bits / crush",
         "Native sample-rate/bit-depth texture. bits is 0..24; crush is "
         "1..128. Preset: bits=8, crush=4.",
         {Arg{"bits", "8"}, Arg{"crush", "4"}}},
        {CatalogKind::NativeControls, "Shape", "Waveshaping",
         "Native waveshaping amount, 0..1.", {Arg{"shape", "0.2"}}},
        {CatalogKind::NativeControls, "Drive", "Drive",
         "Native drive amount, 0..1.", {Arg{"drive", "0.15"}}},
        {CatalogKind::NativeControls, "Chop", "Rhythmic chop",
         "FoxDot chop gate rate, 0..32 pulses per note.",
         {Arg{"chop", "4"}}},
        {CatalogKind::NativeControls, "Spin", "Stereo spin",
         "FoxDot spin moves the stereo position; rate bounded 0..40.",
         {Arg{"spin", "2"}}},
        {CatalogKind::NativeControls, "Coarse", "Coarse",
         "FoxDot coarse pulse-gate texture, 0..32.",
         {Arg{"coarse", "4"}}},
        {CatalogKind::NativeControls, "Formant", "Formant",
         "FoxDot Formlet-style note filter, −1 disables and 0..7 selects "
         "formant positions.", {Arg{"formant", "3"}}},
        {CatalogKind::NativeControls, "Cut", "Note cut",
         "Per-event note cut envelope, bounded 0..1.",
         {Arg{"cut", "0.8"}}},
        {CatalogKind::NativeControls, "Blur", "Blur",
         "FoxDot blur extends note sustain; bounded 0.01..8 beats.",
         {Arg{"blur", "1.5"}}},
        {CatalogKind::NativeControls, "FMod", "FM modulation",
         "Native frequency modulation offset, −2000..2000 Hz.",
         {Arg{"fmod", "0.2"}}},
        {CatalogKind::NativeControls, "LFO filter", "Filter LFO",
         "Filter LFO rate lforate (0..100) and width lfowidth (0..1).",
         {Arg{"lforate", "0.3"}, Arg{"lfowidth", "0.2"}}}
    };
    static const std::vector<std::string> knownExternal{
        "AirwindowsEchojam", "AirwindowsTapeDelay2", "Chorus", "Flanger",
        "Greyhole", "JPverb", "LimutDisperser", "LimutDuck", "LimutFuzz",
        "LimutCompressor", "LimutGatedVerb", "LimutPingPong",
        "LimutShifter", "LimutShimmer", "LimutVocoder", "Phaser",
        "SCLOrkPhazerFX", "SCLOrkDecimatorFX",
        "SCLOrkCrossoverDistortionFX", "Tremolo", "PortedResonatorFX",
        "PortedFverbFX", "PortedAnalogTapeFX", "PortedAnalogChewFX",
        "PortedAnalogDegradeFX", "PortedAnalogLossFX",
        "PortedAnalogPhaserFX", "PortedAnalogVintageDistortionFX",
        "PortedLockhartWavefolderFX", "PortedDCompressorFX",
        "DexterGranularFX"};
    for (const auto& name : knownExternal) {
        const bool loaded = std::find(external.begin(), external.end(), name) !=
                            external.end();
        std::string description;
        const bool hasMixControl =
            name != "AirwindowsEchojam" &&
            name != "AirwindowsTapeDelay2" &&
            name != "LimutVocoder";
        if (name == "AirwindowsEchojam" || name == "AirwindowsTapeDelay2")
            description = "Airwindows player-effect SynthDef. It has no "
                          "fx_mix control; use the documented effect-specific "
                          "fx_* parameters. Install its SynthDef/UGen pack "
                          "under Portable/SynthDefs and restart the DAW.";
        else if (name == "LimutPingPong" || name == "LimutDuck" ||
                 name == "LimutDisperser" || name == "LimutFuzz" ||
                 name == "LimutCompressor" || name == "LimutShifter" ||
                 name == "LimutShimmer" ||
                 name == "LimutGatedVerb")
            description = "Optional Limut-inspired insert UGen. Install its "
                          "SynthDef under Portable/SynthDefs and LimutFX.scx "
                          "under SCPlugins, then restart the DAW. Controls "
                          "include range-checked FX sliders.";
        else if (name == "LimutVocoder")
            description = "Two-input vocoder UGen requiring separate carrier "
                          "and modulator buses. The current player-effect chain "
                          "has no sidechain route, so this is not yet an "
                          "insertable FoxDot effect.";
        else if (name == "SCLOrkPhazerFX" ||
                 name == "SCLOrkDecimatorFX" ||
                 name == "SCLOrkCrossoverDistortionFX")
            description = "Stereo insert adapted from Josh Mitchell's "
                          "SCLOrkSynths patch. It uses SuperCollider core UGens "
                          "only; source and GPL-3.0 license are included with "
                          "the Portable SynthDefs pack.";
        else if (name == "JPverb" || name == "Greyhole")
            description = "Optional stereo room/reverb SynthDef pack. Install "
                          "its Portable/SynthDefs file and required SCPlugins "
                          "module, then restart the DAW. FX Mix controls the "
                          "first insert in a chain.";
        else if (name == "PortedResonatorFX")
            description = "Stereo resonant-body insert from PortedPlugins. "
                          "Install Resonator_scsynth.scx and this SynthDef, "
                          "then restart the DAW. Position and resolution are "
                          "fixed to safe values; the live sliders control mix, "
                          "frequency, structure, brightness and damping.";
        else if (name == "PortedFverbFX")
            description = "Stereo Dattorro-style Fverb insert from "
                          "PortedPlugins. Install Fverb_scsynth.scx and this "
                          "SynthDef. Sliders expose mix, predelay, decay, "
                          "damping frequency and modulation depth.";
        else if (name == "PortedAnalogTapeFX")
            description = "PortedPlugins analog tape saturation insert. "
                          "Uses fixed x2 oversampling and the low-cost RK2 "
                          "solver; sliders control mix, bias, saturation and drive.";
        else if (name == "PortedAnalogChewFX")
            description = "PortedPlugins tape-chew/dropout insert. Sliders "
                          "control mix, chew depth, dropout spacing and variance.";
        else if (name == "PortedAnalogDegradeFX")
            description = "PortedPlugins tape-wear/degradation insert. Sliders "
                          "control mix, depth, wear amount, variance and noise envelope.";
        else if (name == "PortedAnalogLossFX")
            description = "PortedPlugins tape-head-loss filter insert. Sliders "
                          "control gap, tape thickness, spacing and speed. This "
                          "UGen can use high CPU when parameters are modulated.";
        else if (name == "PortedAnalogPhaserFX")
            description = "PortedPlugins virtual-analog feedback phaser insert "
                          "with an internal LFO. Sliders control rate, skew, "
                          "feedback, modulation, stage count and mix.";
        else if (name == "PortedAnalogVintageDistortionFX")
            description = "PortedPlugins Baxandall EQ and vintage distortion "
                          "insert. Oversampling is fixed off for lower CPU; "
                          "sliders control drive, bias, EQ gains and shelf frequency.";
        else if (name == "PortedLockhartWavefolderFX")
            description = "PortedPlugins Lockhart wavefolder insert. Cell count "
                          "is fixed at four; bias and pregain are bounded and "
                          "smoothed, with a safety waveshaper on the wet signal.";
        else if (name == "PortedDCompressorFX")
            description = "PortedPlugins DCompressor insert using its internal "
                          "self-keyed detector. The current player chain does not "
                          "route an external sidechain input.";
        else if (name == "DexterGranularFX")
            description = "Stock-SuperCollider granular insert with four modes: "
                          "0=cloud, 1=stepped slicer, 2=captured freeze, "
                          "3=live-player granulation. "
                          "It processes the selected player's signal; it is not "
                          "an external hardware-input recorder. fx_density is "
                          "grains/second and the freeze mode holds its local "
                          "record buffer when fx_freeze reaches 1.";
        else
            description = "Loaded precompiled stereo player-effect SynthDef. "
                          "Its effect-specific fx_* controls are documented "
                          "in Portable/179-editor-catalog-about.html where "
                          "applicable. FX Mix controls the first chain insert.";
        std::vector<Arg> defaults;
        if (name == "LimutPingPong") {
            defaults = {Arg{"fx_mix", "0.28"}, Arg{"fx_time", "0.32"},
                        Arg{"fx_feedback", "0.45"}};
        } else if (name == "LimutDuck") {
            defaults = {Arg{"fx_mix", "1"}, Arg{"fx_period", "0.5"},
                        Arg{"fx_depth", "0.7"}, Arg{"fx_length", "0.12"},
                        Arg{"fx_attack", "0.18"}};
        } else if (name == "LimutDisperser") {
            defaults = {Arg{"fx_mix", "0.35"}, Arg{"fx_frequency", "440"},
                        Arg{"fx_q", "3"}, Arg{"fx_stages", "24"}};
        } else if (name == "LimutFuzz") {
            defaults = {Arg{"fx_mix", "0.35"}, Arg{"fx_drive", "18"},
                        Arg{"fx_tone", "9000"}};
        } else if (name == "LimutShifter") {
            defaults = {Arg{"fx_mix", "0.5"}, Arg{"fx_ratio", "1.5"},
                        Arg{"fx_window", "0.12"}};
        } else if (name == "PortedResonatorFX") {
            defaults = {Arg{"fx_mix", "0.28"}, Arg{"fx_freq", "220"},
                        Arg{"fx_structure", "0.45"},
                        Arg{"fx_brightness", "0.35"},
                        Arg{"fx_damping", "0.55"}};
        } else if (name == "PortedFverbFX") {
            defaults = {Arg{"fx_mix", "0.3"},
                        Arg{"fx_predelay", "0"},
                        Arg{"fx_decay", "50"},
                        Arg{"fx_damping", "5500"},
                        Arg{"fx_mod_depth", "0.5"}};
        } else if (name == "PortedAnalogTapeFX") {
            defaults = {Arg{"fx_mix", "0.25"}, Arg{"fx_bias", "0.5"},
                        Arg{"fx_saturation", "0.3"}, Arg{"fx_drive", "0.25"}};
        } else if (name == "PortedAnalogChewFX") {
            defaults = {Arg{"fx_mix", "0.25"}, Arg{"fx_depth", "0.2"},
                        Arg{"fx_frequency", "0.5"}, Arg{"fx_variance", "0.1"}};
        } else if (name == "PortedAnalogDegradeFX") {
            defaults = {Arg{"fx_mix", "0.25"}, Arg{"fx_depth", "0.2"},
                        Arg{"fx_amount", "0.2"}, Arg{"fx_variance", "0.1"},
                        Arg{"fx_envelope", "0.25"}};
        } else if (name == "PortedAnalogLossFX") {
            defaults = {Arg{"fx_mix", "0.25"}, Arg{"fx_gap", "0.25"},
                        Arg{"fx_thick", "0.5"}, Arg{"fx_space", "0.35"},
                        Arg{"fx_speed", "0.5"}};
        } else if (name == "PortedAnalogPhaserFX") {
            defaults = {Arg{"fx_mix", "0.3"}, Arg{"fx_rate", "0.25"},
                        Arg{"fx_skew", "0"}, Arg{"fx_feedback", "0.25"},
                        Arg{"fx_modulation", "0.5"}, Arg{"fx_stages", "8"}};
        } else if (name == "PortedAnalogVintageDistortionFX") {
            defaults = {Arg{"fx_mix", "0.3"}, Arg{"fx_drivegain", "0.25"},
                        Arg{"fx_bias", "0"}, Arg{"fx_lowgain", "0.08"},
                        Arg{"fx_highgain", "0.08"},
                        Arg{"fx_shelvingfreq", "600"}};
        } else if (name == "PortedLockhartWavefolderFX") {
            defaults = {Arg{"fx_mix", "0.25"}, Arg{"fx_bias", "0.1"},
                        Arg{"fx_pregain", "2"}};
        } else if (name == "PortedDCompressorFX") {
            defaults = {Arg{"fx_mix", "0.5"}, Arg{"fx_ratio", "4"},
                        Arg{"fx_threshold", "-24"}, Arg{"fx_attack", "0.01"},
                        Arg{"fx_release", "0.1"}, Arg{"fx_makeup", "0.5"},
                        Arg{"fx_automakeup", "1"}};
        } else if (name == "DexterGranularFX") {
            defaults = {Arg{"fx_mix", "0.3"}, Arg{"fx_mode", "0"},
                        Arg{"fx_density", "12"}, Arg{"fx_size", "0.08"},
                        Arg{"fx_pitch", "1"}, Arg{"fx_position", "0.5"},
                        Arg{"fx_freeze", "0"}};
        } else if (name == "LimutShimmer") {
            defaults = {Arg{"fx_mix", "0.35"}, Arg{"fx_ratio", "2"},
                        Arg{"fx_window", "0.12"}, Arg{"fx_room", "0.8"},
                        Arg{"fx_damp", "0.35"}};
        } else if (name == "LimutGatedVerb") {
            defaults = {Arg{"fx_mix", "0.65"}, Arg{"fx_room", "0.75"},
                        Arg{"fx_damp", "0.5"}, Arg{"fx_threshold", "0.02"},
                        Arg{"fx_hold", "0.25"}, Arg{"fx_attack", "0.015"},
                        Arg{"fx_release", "0.15"}};
        } else if (name == "LimutCompressor") {
            defaults = {Arg{"fx_mix", "0.5"}, Arg{"fx_threshold", "-24"},
                        Arg{"fx_ratio", "6"}, Arg{"fx_attack", "0.01"},
                        Arg{"fx_release", "0.1"}, Arg{"fx_knee", "3"}};
        } else if (name == "SCLOrkPhazerFX") {
            defaults = {Arg{"fx_mix", "0.45"}, Arg{"fx_rate", "0.22"},
                        Arg{"fx_depth", "0.65"}, Arg{"fx_center", "650"},
                        Arg{"fx_spread", "0.8"}, Arg{"fx_q", "0.25"},
                        Arg{"fx_tone", "11000"}};
        } else if (name == "SCLOrkDecimatorFX") {
            defaults = {Arg{"fx_mix", "0.4"}, Arg{"fx_rate", "8000"},
                        Arg{"fx_bits", "8"}, Arg{"fx_drive", "1.3"},
                        Arg{"fx_tone", "8000"}};
        } else if (name == "SCLOrkCrossoverDistortionFX") {
            defaults = {Arg{"fx_mix", "0.35"}, Arg{"fx_drive", "2.0"},
                        Arg{"fx_shape", "0.75"}, Arg{"fx_tone", "9000"}};
        } else if (hasMixControl) {
            defaults.push_back(Arg{"fx_mix", "0.25"});
        }
        const auto label = name == "PortedResonatorFX"
            ? std::string("Resonator (Ported)")
            : name == "PortedFverbFX" ? std::string("Fverb (Ported)")
            : name == "PortedAnalogTapeFX" ? std::string("Analog Tape")
            : name == "PortedAnalogChewFX" ? std::string("Analog Chew")
            : name == "PortedAnalogDegradeFX" ? std::string("Analog Degrade")
            : name == "PortedAnalogLossFX" ? std::string("Analog Loss")
            : name == "PortedAnalogPhaserFX" ? std::string("Analog Phaser")
            : name == "PortedAnalogVintageDistortionFX"
                ? std::string("Vintage Distortion")
            : name == "PortedLockhartWavefolderFX"
                ? std::string("Lockhart Wavefolder")
            : name == "PortedDCompressorFX" ? std::string("DCompressor")
            : name == "DexterGranularFX" ? std::string("Granular FX")
                                             : name;
        result.push_back({CatalogKind::ExternalEffect, name, label,
                          std::move(description), std::move(defaults), loaded});
    }
    for (auto& entry : result) {
        entry.category = effectCategory(entry);
        entry.subcategory = effectSubcategory(entry);
    }
    std::sort(result.begin(), result.end(), [](const auto& left,
                                               const auto& right) {
        return std::make_tuple(left.category, left.subcategory,
                               catalogLower(left.label)) <
               std::make_tuple(right.category, right.subcategory,
                               catalogLower(right.label));
    });
    return result;
}

} // namespace dexter::editor
