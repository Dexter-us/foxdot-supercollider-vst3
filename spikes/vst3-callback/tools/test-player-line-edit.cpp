#include "../player_line_edit.h"
#include "../live_player_controls.h"
#include "../editor_catalog.h"

#include <cassert>
#include <cmath>
#include <string>

using namespace dexter::editor;

int main() {
    assert(isSimpleIdentifier("CustomPluck"));
    assert(!isSimpleIdentifier("Airwindows-Echojam"));
    assert(clampEditorPlayerControl(EditorPlayerControl::Amplitude, 1.5f) ==
           1.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::Pan, -2.0f) == -1.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::Cutoff, 0.0f) ==
           20.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::EffectMix, 0.4f) ==
           0.4f);
    assert(clampEditorPlayerControl(EditorPlayerControl::BitDepth, -1.0f) ==
           0.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::BitDepth, 32.0f) ==
           24.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::CrushRate, 0.0f) ==
           1.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::CrushRate, 256.0f) ==
           128.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::PShift, -30.0f) ==
           -24.0f);
    assert(clampEditorPlayerControl(EditorPlayerControl::PShift, 30.0f) ==
           24.0f);
    assert(!editorPlayerControlAvailable(
        EditorPlayerControl::BitDepth, false, false));
    assert(editorPlayerControlAvailable(
        EditorPlayerControl::BitDepth, false, true));
    assert(editorPlayerControlAvailable(
        EditorPlayerControl::CrushRate, false, true));
    assert(editorPlayerControlAvailable(
        EditorPlayerControl::PShift, false, false));
    assert(editorPlayerControlVisible(
        EditorPlayerControl::PShift, true, false, false));
    assert(editorPlayerControlVisible(
        EditorPlayerControl::PShift, true, true, false));
    assert(!editorPlayerControlVisible(
        EditorPlayerControl::PShift, false, true, false));
    assert(editorPlayerControlVisible(
        EditorPlayerControl::CrushRate, true, false, false));
    assert(!editorPlayerControlVisible(
        EditorPlayerControl::CrushRate, true, true, false));
    assert(editorPlayerControlVisible(
        EditorPlayerControl::CrushRate, true, true, true));
    assert(smoothEditorPlayerControl(0.0f, 12.0f, 0.25f) == 3.0f);
    assert(smoothEditorPlayerControl(12.0f, 0.0f, 1.0f) == 0.0f);
    assert(!editorPlayerControlAvailable(
        EditorPlayerControl::EffectMix, false, true));
    assert(std::abs(smoothEditorPlayerControl(0.0f, 1.0f, 0.25f) - 0.25f) <
           1e-6f);
    {
        std::vector<std::string> embedded{
            "sine", "square", "pluck", "sample", "sawbass"};
        std::vector<std::string> external{"CustomPluck", "Chorus"};
        const auto instruments = makeInstrumentCatalog(embedded, external);
        assert(std::is_sorted(instruments.begin(), instruments.end(),
            [](const auto& left, const auto& right) {
                if (left.category != right.category)
                    return left.category < right.category;
                auto a = left.label;
                auto b = right.label;
                std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return a < b;
            }));
        assert(std::any_of(instruments.begin(), instruments.end(),
            [](const auto& entry) {
                return entry.name == "sawbass" && entry.category == "Bass";
            }));
        assert(std::none_of(instruments.begin(), instruments.end(),
            [](const auto& entry) { return entry.name == "square"; }));
        assert(std::any_of(instruments.begin(), instruments.end(),
            [](const auto& entry) { return entry.name == "pulse"; }));
        const auto chorus = std::find_if(instruments.begin(), instruments.end(),
            [](const auto& entry) { return entry.name == "Chorus"; });
        assert(chorus == instruments.end());
        const auto custom = std::find_if(instruments.begin(), instruments.end(),
            [](const auto& entry) { return entry.name == "CustomPluck"; });
        assert(custom != instruments.end() &&
               custom->kind == CatalogKind::UserSynth);
        const auto effects = makeEffectCatalog(external);
        assert(effects.size() >= 28);
        assert(std::is_sorted(effects.begin(), effects.end(),
            [](const auto& left, const auto& right) {
                if (left.category != right.category)
                    return left.category < right.category;
                auto a = left.label;
                auto b = right.label;
                std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return a < b;
            }));
        assert(std::any_of(effects.begin(), effects.end(),
            [](const auto& entry) {
                return entry.name == "AirwindowsEchojam" &&
                       entry.category == "Delay";
            }));
        const auto loadedChorus = std::find_if(effects.begin(), effects.end(),
            [](const auto& entry) { return entry.name == "Chorus"; });
        const auto unloadedJPverb = std::find_if(effects.begin(), effects.end(),
            [](const auto& entry) { return entry.name == "JPverb"; });
        assert(loadedChorus != effects.end() && loadedChorus->available);
        assert(unloadedJPverb != effects.end() && !unloadedJPverb->available);
    }
    {
        std::string source = "# blank player\np3 >>\n";
        PlayerAssignment assignment;
        assert(findPlayerLineAt(source, source.find(">>") + 2,
                                assignment, true));
        CatalogEntry instrument{CatalogKind::Instrument, "pluck", "pluck",
                                "built-in", {}, true};
        std::string error;
        assert(applyCatalogEntry(source, source.find("p3"), instrument, error));
        assert(source.find("p3 >> pluck(P[0, 2, 4, 7], dur=0.5, amp=0.25)") !=
               std::string::npos);
    }
    {
        std::string source = "p3 >>\n";
        CatalogEntry chorus{CatalogKind::ExternalEffect, "Chorus", "Chorus",
                            "loaded", {{"fx_mix", "0.25"}}, true};
        std::string error;
        assert(applyCatalogEntry(source, 1, chorus, error));
        assert(source.find("sine(P[0, 2, 4, 7], dur=0.5, amp=0.25, "
                           "effect=\"Chorus\", fx_mix=0.25)") !=
               std::string::npos);
    }
    {
        std::string source =
            "p3 >> sine(P[0, 2, 4], effects=[(\"Chorus\", {})])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 1, assignment));
        CatalogEntry flanger{CatalogKind::ExternalEffect, "Flanger", "Flanger",
                             "loaded", {{"fx_mix", "0.25"}}, true};
        std::string error;
        assert(applyCatalogEntry(source, 1, flanger, error));
        assert(source.find("P[0, 2, 4], effects=[(\"Chorus\", {}), "
                           "(\"Flanger\", {})]") != std::string::npos);
    }
    {
        std::string source =
            "p1 >> sine(0, effects=[(\"Chorus\", {}), (\"Flanger\", {}), "
            "(\"Phaser\", {}), (\"Tremolo\", {})])\n";
        const auto original = source;
        CatalogEntry chorus{CatalogKind::ExternalEffect, "JPverb", "JPverb",
                            "loaded", {{"fx_mix", "0.25"}}, true};
        std::string error;
        assert(!applyCatalogEntry(source, 1, chorus, error));
        assert(source == original);
        assert(error.find("four effect") != std::string::npos);
    }
    float level = 0.0f;
    const float smoothing = 1.0f - std::exp(-1.0f / (44100.0f * 0.01f));
    for (int index = 0; index < 4410; ++index) {
        const float next = smoothEditorPlayerControl(level, 1.0f, smoothing);
        assert(next >= level && next - level < 0.003f);
        level = next;
    }
    assert(level > 0.999f);
    {
        const std::string source =
            "p1 >> saw(P[0, 2], dur=var([0.25, 0.5], 4))\n"
            "p9 >> sine(0)\n"
            "player >> pluck(0)\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 7, assignment));
        assert(assignment.playerIndex == 0);
        assert(!findPlayerAssignmentAt(source, source.find("p9"), assignment));
        assert(!findPlayerAssignmentAt(source, source.find("player"), assignment));
    }
    {
        std::string source = "p2 >> saw(P[0, 2], dur=2, pan=0.25)  # keep\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 5, assignment));
        assert(replacePlayerSynth(source, assignment, "triangle"));
        assert(source.find("p2 >> triangle(") == 0);
        assert(source.find("dur=2, pan=0.25") != std::string::npos);
        assert(source.find("# keep") != std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(P[0, 2], dur=var([0.25, 0.5], 4), amp=var([0.2, 0.4], 2))\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 2, assignment));
        assert(setPlayerArgument(source, assignment, "amp", "0.65"));
        assert(source.find("amp=0.65") != std::string::npos);
        assert(source.find("dur=var([0.25, 0.5], 4)") != std::string::npos);
        assert(source.find("var([0.2, 0.4], 2)") == std::string::npos);
    }
    {
        std::string source = "p3 >> pluck(P[0, 4], dur=1)\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerArgument(source, assignment, "cutoff", "1800"));
        assert(source.find("dur=1, cutoff=1800") != std::string::npos);
    }
    {
        std::string source =
            "p2 >> saw(P[0, 3], bits=var([0, 10, 6], 4), "
            "crush=var([1, 4, 12], 4))\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentForIndex(source, 1, assignment));
        assert(setPlayerArgument(source, assignment, "bits", "8"));
        assert(source.find("bits=8") != std::string::npos);
        assert(source.find("bits=var(") == std::string::npos);
        assert(source.find("crush=var([1, 4, 12], 4)") != std::string::npos);
        assert(findPlayerAssignmentForIndex(source, 1, assignment));
        assert(setPlayerArgument(source, assignment, "crush", "6"));
        assert(source.find("crush=6") != std::string::npos);
        assert(source.find("bits=8") != std::string::npos);
    }
    {
        std::string source = "p1 >> saw(P[0, 2], effect=\"Phaser\", fx_mix=0.2)\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffect(source, assignment, "Tremolo"));
        assert(source.find("effect=\"Tremolo\"") != std::string::npos);
        assert(source.find("fx_mix=0.2") != std::string::npos);
    }
    {
        std::string source = "p1 >> saw(P[0], effects=[])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffect(source, assignment, "Phaser"));
        assert(source.find("effects=[(\"Phaser\", {})]") != std::string::npos);
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffect(source, assignment, "Tremolo"));
        assert(source.find("(\"Phaser\", {}), (\"Tremolo\", {})") !=
               std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(0, effects=[(\"Chorus\", {})])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffectMix(source, assignment, "0.72"));
        assert(source.find("{\"fx_mix\": 0.72}") != std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(0, effects=[(\"Chorus\", {\"fx_rate\": 2})])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffectMix(source, assignment, "0.4"));
        assert(source.find("{\"fx_rate\": 2, \"fx_mix\": 0.4}") !=
               std::string::npos);
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffectMix(source, assignment, "0.8"));
        assert(source.find("{\"fx_rate\": 2, \"fx_mix\": 0.8}") !=
               std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(0, effects=[(\"A\", {}), (\"B\", {}), "
            "(\"C\", {}), (\"D\", {})])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(!setPlayerEffect(source, assignment, "Tremolo"));
        assert(source.find("(\"Tremolo\", {})") == std::string::npos);
    }
    {
        std::string source =
            "p1 >> pluck(P[0, 4],\n    dur=0.5,\n)\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerArgument(source, assignment, "amp", "0.4"));
        assert(source.find("amp=0.4)") != std::string::npos);
        assert(source.find(", ,") == std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(0, effects=[('Chorus', {'fx_mix': var([0.2, 0.8], 4), "
            "'fx_rate': 2}), ('Tremolo', {'fx_mix': 0.9})])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffectMix(source, assignment, "0.5"));
        assert(source.find("'fx_mix': 0.5, 'fx_rate': 2") !=
               std::string::npos);
        assert(source.find("('Tremolo', {'fx_mix': 0.9})") !=
               std::string::npos);
    }
    {
        std::string source =
            "p1 >> saw(0, effects=[('Chorus', {'fx_rate': 2,}),])\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffectMix(source, assignment, "0.5"));
        assert(source.find("'fx_rate': 2, \"fx_mix\": 0.5") !=
               std::string::npos);
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(setPlayerEffect(source, assignment, "Tremolo"));
        assert(source.find("}), (\"Tremolo\", {})]") !=
               std::string::npos);
    }
    {
        std::string source = "p1 >> saw(0)\n";
        PlayerAssignment assignment;
        assert(findPlayerAssignmentAt(source, 0, assignment));
        assert(!setPlayerEffect(source, assignment, "Bad Name"));
    }
    return 0;
}
