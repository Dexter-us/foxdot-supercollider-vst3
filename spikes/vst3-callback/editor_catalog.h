#pragma once

#include "player_line_edit.h"

#include <algorithm>
#include <cctype>
#include <string>
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

    CatalogEntry() = default;
    CatalogEntry(CatalogKind kindValue, std::string nameValue,
                 std::string labelValue, std::string aboutValue,
                 std::vector<CatalogArgument> argumentsValue = {},
                 bool availableValue = true,
                 std::string categoryValue = {})
        : kind(kindValue), name(std::move(nameValue)),
          label(std::move(labelValue)), about(std::move(aboutValue)),
          arguments(std::move(argumentsValue)), available(availableValue),
          category(std::move(categoryValue)) {}
};

inline std::string instrumentCategory(const CatalogEntry& entry) {
    if (entry.kind == CatalogKind::SamplePlayer)
        return "Sample player";
    if (entry.kind == CatalogKind::UserSynth)
        return "Portable SynthDefs";
    static const std::vector<std::string> bass{
        "bass", "dbass", "jbass", "sawbass"};
    static const std::vector<std::string> keys{
        "bell", "glass", "gong", "keys", "klank", "marimba", "pads",
        "sinepad"};
    static const std::vector<std::string> plucked{
        "donk", "karp", "nylon", "pluck", "sitar", "squish", "twang",
        "viola"};
    static const std::vector<std::string> texture{
        "ambi", "creep", "dirt", "fuzz", "noise", "orient", "scratch",
        "scatter", "space", "spark", "swell"};
    if (std::find(bass.begin(), bass.end(), entry.name) != bass.end())
        return "Bass";
    if (std::find(keys.begin(), keys.end(), entry.name) != keys.end())
        return "Keys and mallets";
    if (std::find(plucked.begin(), plucked.end(), entry.name) != plucked.end())
        return "Plucked and strings";
    if (std::find(texture.begin(), texture.end(), entry.name) != texture.end())
        return "Texture and noise";
    return "Synth voices";
}

inline std::string effectCategory(const CatalogEntry& entry) {
    const auto& name = entry.name;
    if (name == "Echo" || name == "AirwindowsEchojam" ||
        name == "AirwindowsTapeDelay2")
        return "Delay";
    if (name == "Freeverb" || name == "Freeverb2" || name == "GVerb" ||
        name == "Greyhole" || name == "JPverb")
        return "Reverb";
    if (name == "Chorus" || name == "Flanger" || name == "Phaser" ||
        name == "Tremolo" || name == "Chop" || name == "Spin")
        return "Modulation and rhythm";
    if (name == "Low-pass" || name == "High-pass" ||
        name == "Band-pass" || name == "LFO filter")
        return "Filters";
    if (name == "Vibrato" || name == "Slide" || name == "Bend" ||
        name == "PShift" || name == "Coarse" || name == "Formant")
        return "Pitch and formant";
    if (name == "Bits / Crush" || name == "Shape" || name == "Drive")
        return "Distortion and reduction";
    if (name == "Cut" || name == "Blur" || name == "FMod")
        return "Player controls";
    return "Other effects";
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
        "Greyhole", "JPverb", "Phaser", "Tremolo"};
    for (const auto& name : external) {
        if (std::find(bundledEffects.begin(), bundledEffects.end(), name) !=
                bundledEffects.end() ||
            !isSimpleIdentifier(name) ||
            std::any_of(result.begin(), result.end(), [&](const auto& item) {
                return item.name == name;
            }))
            continue;
        result.push_back({CatalogKind::UserSynth, name,
            name + " [Portable SynthDef]",
            "A precompiled user SynthDef loaded from Portable/SynthDefs/" +
                name +
                ".scsyndef. It does not load new UGen binaries; those require "
                "an installed compatible SCPlugins module and host restart.",
            {}, true});
    }
    for (auto& entry : result)
        entry.category = instrumentCategory(entry);
    const auto less = [](const auto& left, const auto& right) {
        auto a = left.label;
        auto b = right.label;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return std::tie(left.category, a) < std::tie(right.category, b);
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
         "Per-note pitch shift in semitones, bounded −24..24.",
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
        "Greyhole", "JPverb", "Phaser", "Tremolo"};
    for (const auto& name : knownExternal) {
        const bool loaded = std::find(external.begin(), external.end(), name) !=
                            external.end();
        std::string description;
        const bool hasMixControl =
            name != "AirwindowsEchojam" &&
            name != "AirwindowsTapeDelay2";
        if (name == "AirwindowsEchojam" || name == "AirwindowsTapeDelay2")
            description = "Airwindows player-effect SynthDef. It has no "
                          "fx_mix control; use the documented effect-specific "
                          "fx_* parameters. Install its SynthDef/UGen pack "
                          "under Portable/SynthDefs and restart the DAW.";
        else if (name == "JPverb" || name == "Greyhole")
            description = "Optional stereo room/reverb SynthDef pack. Install "
                          "its Portable/SynthDefs file and required SCPlugins "
                          "module, then restart the DAW. FX Mix controls the "
                          "first insert in a chain.";
        else
            description = "Loaded precompiled stereo player-effect SynthDef. "
                          "Its effect-specific fx_* controls are documented "
                          "in Portable/179-editor-catalog-about.html where "
                          "applicable. FX Mix controls the first chain insert.";
        std::vector<Arg> defaults;
        if (hasMixControl)
            defaults.push_back(Arg{"fx_mix", "0.25"});
        result.push_back({CatalogKind::ExternalEffect, name, name,
                          std::move(description), std::move(defaults), loaded});
    }
    for (auto& entry : result)
        entry.category = effectCategory(entry);
    std::sort(result.begin(), result.end(), [](const auto& left,
                                               const auto& right) {
        auto a = left.label;
        auto b = right.label;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return std::tie(left.category, a) < std::tie(right.category, b);
    });
    return result;
}

} // namespace dexter::editor
