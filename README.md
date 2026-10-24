# Dexter U.S. FoxDot VST3

A Windows x64 VST3 prototype for Python-based live-coded music, using a small FoxDot-inspired pattern API and an embedded SuperCollider synthesis engine.

## MVP decisions

- **Manufacturer:** Dexter U.S.
- **Plugin:** Windows x64 VST3 with a native code editor, SuperCollider audio output, and MIDI event output.
- **Code:** Bundled CPython 3.12 runtime with a deliberately small, FoxDot-style pattern/player API. This is not full FoxDot compatibility.
- **Synthesis:** SuperCollider's synthesis engine runs in the plugin process. No separate SuperCollider application/server process.
- **Control path:** Python evaluates on a worker thread; a bounded event pattern is handed directly to the audio callback. No OSC or external server.
- **Tempo:** Each pattern has its own tempo, defaulting to 120 BPM. Timing is controlled by `tempo(bpm)`, independent of DAW transport.

## Current prototype

- Windows native editor with Run, Stop, Open, and Save. Six starter patterns are separate `.py` files in the `Portable` folder.
- Open and Save As explicitly start in the `Portable` folder beside the VST3 bundle; ordinary Save writes back to the currently opened or saved script.
- The current script is stored in VST3 project state. Restoring a project restores the code but does not execute it automatically.
- `tempo(bpm)`, `scale(name="major", root=60)`, and `play(notes, step=1.0, duration=None, velocity=100, offset=0.0, degrees=False)`. Pattern helpers include `reverse`, `rotate`, `repeat`, `transpose`, and `euclid`.
- Raw MIDI pitches remain the default. With `degrees=True`, zero-based scale degrees map to pitches using major, minor, dorian, mixolydian, major/minor pentatonic, or blues scales; degrees wrap across octaves.
- MIDI note numbers 0ÃÂ¢ÃÂÃÂ127, `None` rests, repeating patterns, and up to 8 simultaneous audio voices.
- Chords can be written as nested lists, such as `play([[60, 64, 67], [62, 65, 69]])`; separate `play()` calls can layer parts.
- Maximum 256 note events per pattern, 256 step slots, 4096 beats, and 20ÃÂ¢ÃÂÃÂ300 BPM.
- Embedded CPython runtime and the required SuperCollider UGen modules are packaged with the VST3.
- MIDI note events are emitted on a VST3 event output bus; the pattern supports up to 8 simultaneous notes, with extra audio voices reserved for short release tails.
- Internal audio applies MIDI velocity and a per-voice 5 ms attack / 12 ms release envelope, then normalizes the chord mix to keep level reasonably consistent.
- The plugin display name and native editor branding read ÃÂ¢ÃÂÃÂDexter U.S FoxDotÃÂ¢ÃÂÃÂ; the VST3 manufacturer metadata remains Dexter U.S.
- Dexter U.S. manufacturer metadata.

Example:

```python
tempo(120)
play([60, 64, None, 67], step=0.5, duration=0.4)
```

Chord example:

```python
tempo(100)
play([[60, 64, 67], [57, 60, 64], [53, 57, 60], [55, 59, 62]],
     step=1, duration=0.9)
```

Scale-degree example:

```python
tempo(112)
scale("minor", root=60)
play([0, 2, 4, 6, 4, 2], step=0.5, duration=0.4, degrees=True)
```

This is only a pattern-and-pitch slice: user code does not define arbitrary SuperCollider SynthDefs or access the full FoxDot API. The Python worker has a 2-second guard for ordinary Python loops and a restricted built-ins set, but this is not a security sandbox; only run code you trust.

## Not included yet

- Full compatibility with all FoxDot syntax, classes, synths, samples, and editor behavior.
- An external SuperCollider process, network OSC, or remote server control.
- DAW tempo/transport sync, MIDI clock, or network synchronization.
- User-defined SynthDefs, sample playback, MIDI input, full FoxDot compatibility, and macOS/Linux editor support.

## Architecture

`VST3 host audio callback -> embedded SuperCollider engine -> host audio outputs`

`Code editor -> embedded Python worker -> bounded pattern snapshot -> host audio callback -> SuperCollider + VST3 MIDI output`

## Milestones

1. **Complete:** Render the embedded SuperCollider engine in a VST3 process, with no separate server or OSC.
2. **Complete:** Bundle the required SuperCollider UGen modules and license texts.
3. **Complete in prototype:** Add Windows editor, embedded Python, repeating note patterns, and VST3 MIDI output.
4. **In progress:** Validate the Python editor/runtime and MIDI/audio behavior in a Windows DAW; the user can test on Windows only.
5. **In progress:** Save and restore editor code with DAW project state.
6. **Later:** Incrementally expand the FoxDot-inspired pattern API; full FoxDot compatibility is not assumed.

## Licensing and upstream code

No FoxDot source has been copied into this repository. The VST3 bundles SuperCollider under GPL-3.0, the Python runtime under the PSF license, and the Steinberg VST3 SDK under its MIT license; notices are included with the package. Distribution must meet the applicable source-offer, notices, and dependency-license requirements. Review the complete dependency set before commercial redistribution.
