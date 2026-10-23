# Dexter U.S. FoxDot VST3

A Windows x64 VST3 prototype for Python-based live-coded music, using a small FoxDot-inspired pattern API and an embedded SuperCollider synthesis engine.

## MVP decisions

- **Manufacturer:** Dexter U.S.
- **Plugin:** Windows x64 VST3 with a native code editor, SuperCollider audio output, and MIDI event output.
- **Code:** Bundled CPython 3.12 runtime with a deliberately small, FoxDot-style pattern/player API. This is not full FoxDot compatibility.
- **Synthesis:** SuperCollider's synthesis engine runs in the plugin process. No separate SuperCollider application/server process.
- **Control path:** Python evaluates on a worker thread; a bounded event pattern is handed directly to the audio callback. No OSC or external server.
- **Tempo:** Each pattern uses its own tempo, defaulting to 120 BPM. DAW transport synchronization is out of scope for this first slice.

## Current prototype

- Windows native editor with Run, Stop, and a status/error line.
- `tempo(bpm)` and `play(notes, step=1.0, duration=None, velocity=100, offset=0.0)`.
- MIDI note numbers 0–127, `None` rests, repeating patterns, and one monophonic voice.
- Maximum 256 note slots per pattern, 4096 beats, and 20–300 BPM.
- Embedded CPython runtime and the required SuperCollider UGen modules are packaged with the VST3.
- MIDI note events are emitted on a VST3 event output bus; SuperCollider renders the same single-voice pattern to audio.
- Dexter U.S. manufacturer metadata.

Example:

```python
tempo(120)
play([60, 64, None, 67], step=0.5, duration=0.4)
```

This is only a pattern-and-pitch slice: user code does not define arbitrary SuperCollider SynthDefs or access the full FoxDot API. The Python worker has a 2-second guard for ordinary Python loops and a restricted built-ins set, but this is not a security sandbox; only run code you trust.

## Not included yet

- Full compatibility with all FoxDot syntax, classes, synths, samples, and editor behavior.
- An external SuperCollider process, network OSC, or remote server control.
- DAW transport/tempo following, MIDI clock, or network synchronization.
- Polyphony, user-defined SynthDefs, sample playback, MIDI input, presets, plugin state persistence, and macOS/Linux editor support.

## Architecture

`VST3 host audio callback -> embedded SuperCollider engine -> host audio outputs`

`Code editor -> embedded Python worker -> bounded pattern snapshot -> host audio callback -> SuperCollider + VST3 MIDI output`

## Milestones

1. **Complete:** Render the embedded SuperCollider engine in a VST3 process, with no separate server or OSC.
2. **Complete:** Bundle the required SuperCollider UGen modules and license texts.
3. **Complete in prototype:** Add Windows editor, embedded Python, repeating note patterns, and VST3 MIDI output.
4. **In progress:** Validate the Python editor/runtime and MIDI/audio behavior in a Windows DAW; the user can test on Windows only.
5. **Later:** Add transport sync, polyphony, persistent code/presets, and broader FoxDot-like features.

## Licensing and upstream code

No FoxDot source has been copied into this repository. The VST3 bundles SuperCollider under GPL-3.0, the Python runtime under the PSF license, and the Steinberg VST3 SDK under its MIT license; notices are included with the package. Distribution must meet the applicable source-offer, notices, and dependency-license requirements. Review the complete dependency set before commercial redistribution.
