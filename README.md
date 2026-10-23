# Dexter U.S. FoxDot VST3

A Windows x64 VST3 prototype for Python-based live-coded music, using a small FoxDot-inspired language surface and SuperCollider synthesis.

## MVP decisions

- **Manufacturer:** Dexter U.S.
- **Plugin:** Windows x64 VST3 with SuperCollider audio output and MIDI event output.
- **Code:** Embedded Python runtime with a deliberately small, FoxDot-style pattern/player API. This is not a promise of full FoxDot compatibility.
- **Synthesis:** SuperCollider's synthesis engine runs in the plugin process. No separate SuperCollider application/server process.
- **Control path:** No OSC in the intended MVP. A direct in-process C++ bridge from the Python/control side to the SuperCollider engine must be prototyped and validated.
- **Tempo:** Plugin-owned tempo clock for the first version. DAW transport synchronization is out of scope initially.

## First-version feature scope

- A code editor with evaluate/update, stop/clear, and error/status output.
- Internal tempo, initially 120 BPM, with a tempo control in code or UI.
- A small set of pattern/player operations for rhythmic and pitched events.
- A small bundled set of SuperCollider instruments and sample assets.
- MIDI event output in addition to the plugin's SuperCollider-rendered audio.
- Dexter U.S. manufacturer metadata in the VST3.

## Explicitly out of scope for the MVP

- Full compatibility with all FoxDot syntax, classes, synths, samples, and editor behavior.
- An external SuperCollider process, network OSC, or remote server control.
- DAW transport/tempo following, MIDI clock, or network synchronization.
- Reaper/Ableton-specific backends, collaboration features, large sample libraries, and third-party SuperCollider plugin packs.

## Architecture to validate

`VST3 host audio callback -> embedded SuperCollider engine -> host audio outputs`

`Code editor -> embedded Python -> internal event/command queue -> SuperCollider + VST3 MIDI output`

Python evaluation must stay off the real-time audio thread. The bridge must hand off bounded, timestamped events safely to the audio side. The direct bridge is a design hypothesis until a working prototype proves it; SuperCollider's normal control path is OSC-based.

## Milestones

1. **Engine spike:** Build a minimal VST3 that renders one embedded SuperCollider SynthDef into the host audio buffers, without launching a separate server process.
2. **Direct-control spike:** Trigger and update one synth from an in-process C++ command/event bridge, with no OSC packets or sockets.
3. **MIDI spike:** Emit timed MIDI note events from the same pattern scheduler while audio continues to render.
4. **Python slice:** Embed Python and support one small live-coding pattern end-to-end; add stop/clear and plugin-state persistence.
5. **Packaging and validation:** Build/install in a Windows DAW, then test timing, audio underruns, MIDI timing, plugin lifecycle, and multiple instances.

## Licensing and upstream code

No FoxDot or SuperCollider source has been copied into this repository. Review FoxDot and SuperCollider licensing and distribution requirements before incorporating or redistributing upstream code, binaries, or assets. The repository does not yet declare a project license.
