#include "../player_line_edit.h"
#include "../live_player_controls.h"

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
    assert(std::abs(smoothEditorPlayerControl(0.0f, 1.0f, 0.25f) - 0.25f) <
           1e-6f);
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
