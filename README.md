# Dexter U.S. FoxDot VST3

A Windows x64 VST3 prototype for Python-based live-coded music, using a small FoxDot-inspired pattern API and an embedded SuperCollider synthesis engine.

Version 0.4.193 adds three PortedPlugins resonator voices, a stereo Resonator
insert, and the pack's Fverb stereo reverb insert. Examples 232b–234 cover safe
octave checks, the modal voices, and the Resonator/Fverb inserts alongside
JPverb. The optional Windows add-on includes the corresponding wrappers and
slider controls with their required PortedPlugins `.scx` modules.

Version 0.4.192 added eight curated PortedPlugins player wrappers: two analog
drums, five oscillator voices, and a plucked-string voice. The separate Windows
add-on keeps their `.scsyndef` files and bounded `.controls` sidecars together
with the required PortedPlugins `.scx` modules. Example 232 covers those voices.

Version 0.4.191 adds finite pattern-valued `sleep()` and Sardine-style
oversleeping inside bounded `@swim`: `sleep(P[0.5, 1.25])` selects by cycle
iterator `i`, and later events may land after the next cycle starts. Expanded
events are kept in chronological order; each sample hit remains paired with its
sample request. Per-call and total offsets, event counts, and the 4096-beat
timeline stay bounded. This does not add Sardine's SPL/Ziffers/Vortex languages
or its `D()`/`N()` senders; see Example 231.

Version 0.4.190 adds bounded ordered chains of up to two per-note inserts:
`fx_event=P[("Chorus", "Greyhole"), None, ...]` applies each pair in order,
and seeded `PRand` can choose among such chains. `fx_event_mix` is broadcast
to both inserts; their other controls use SynthDef defaults. This is a
PbindFx-inspired subset, not a complete clone; see Example 230.
Version 0.4.189 adds a smoothed `fx_mix` dry/wet control to Airwindows
TapeDelay2 and Echojam, and lets seeded `PRand` choose between installed
per-note effects. Example 229 demonstrates reproducible random delay choices
and a separately seeded per-note wet amount.
Version 0.4.188 lets `fx_event` select installed delay and reverb SynthDefs per
note, including JPverb, Greyhole, and selected Limut effects.
Each note has its own stereo insert graph; tails process zero input until quiet
or a 12-second cap, with at most four tails per player. It also fixes the sclang
audio-bus range mismatch exposed by 0.4.187's Windows integration test. Example
228 demonstrates the per-note delay/reverb selector.

Version 0.4.186 makes the script worker's state visible: Run changes to a
disabled Busy button until the current request finishes, and duplicate Run
attempts receive a wait-for-ready message rather than a misleading compatibility
error. PShift is now a per-player control; its slider is hidden until a selected
player actually uses `pshift`.

Version 0.4.146 keeps the full player-control row visible and disabled until a
player is selected, so PShift is discoverable. Clicking a `pN >>` assignment
in the editor selects that player in the Players panel. Example 184 demonstrates
the PShift and bitcrusher controls together.

Version 0.4.145 adds bounded `PIndex`, seeded `PxRand` (no adjacent
repeats), and seeded weighted `PwRand`, with Portable Example 183. Synth and
Effect family categories now start collapsed, and their first click expands
them.

Version 0.4.144 adds the FoxDot finite generators `P10`, `PBern`, `PSine`,
`PTri`, and `PSum`; corrects the compatibility checker so `.expand()` is
accepted on `Pvar` but not ordinary finite `P` patterns; adds a live PShift
semitone slider for any selected player; and makes the Synths/Effects category
headings collapsible. Examples 181 and 182 demonstrate the generators and
PShift slider.

Version 0.4.143 groups the Synths and Effects catalogs by family and adds
player-specific Bits and Crush sliders. See Example 180.

Version 0.4.142 expands the editor catalog: sorted instruments, a fuller native
Effects list, right-click Insert/About actions, and drag-and-drop onto player
lines—including a blank `p3 >>` starter. About opens a bundled HTML reference
with effect ranges and optional-pack notes. Example 179 demonstrates the workflow.
Catalog actions edit the script; click Run to apply. Audio remains in the VST's
local engine, with no OSC/server change.

Version 0.4.79 adds beat-synced `var()`/`linvar()` modulation for the
per-player Airwindows TapeDelay2 and Echojam controls, with 15 ms smoothing for
changes and Portable Example 105. The current sample pack and UGen requirements
are unchanged.

Version 0.4.68 adds `Clock.swing(amount=0.1)`, matching FoxDot's alternating
half-beat clock nudge in the plugin's sample-accurate scheduler; zero disables
it. The shift is safety-bounded to less than half a beat; its phase starts when
the player is run or updated, not from DAW transport. See Example 93.
Version 0.4.67 extends `PZip(...)` and `PZip2(...)` to finite sample-symbol
patterns, cycling inputs to their least-common-multiple length and flattening
them into simultaneous hits. The laced result is capped at 256 items and each
hit at eight symbols; numeric PZip behavior is unchanged. See Example 92 and
`Portable/OPTIONAL-SAMPLE-SETUP.html` for the optional sample-folder guide.
Version 0.4.66 added FoxDot-style `P & P` chord-zipping for finite sample-symbol
patterns; see Example 91. Version 0.4.65
adds FoxDot-style grouped `sus=` values using `PGroup(...)`;
groups are laced with chord tones and rhythm offsets within the eight-voice and
256-event caps; see Example 90. Version 0.4.64 added bounded nested
`PRhythm(...)` groups with nesting limited to eight levels; see Example 89.
Version 0.4.63 extends
`PAlt`, `PStretch`, `PStutter`, `PShuf`, and `PJoin`
to finite numeric note patterns, including rests and chord groups, as well as
sample-symbol patterns. `PStutter` now uses FoxDot's least-common-multiple
cycling for source/count patterns; see Example 88. Version 0.4.62 allows
FoxDot-style overlapping sustains and repeated pitches,
with a bounded eight-voice safety limit; see Example 87. Version 0.4.61 added
bounded numeric-note `PZip(...)` and filtered `PZip2(...)`
chord patterns; see Example 86. Version 0.4.60 added FoxDot-style
`PQuicken(...)` delay clusters and a bounded `PRhythm(...)` subset; see Example 85.
Version 0.4.59 added bounded
`PDelay(...)` beat offsets and FoxDot's `PStrum(...)` rhythm pattern; Example 84
combines them. Version 0.4.58 matches FoxDot's
slice-based `start` rotations for `PBeat(...)` and `PDur(...)`, including
negative and over-length offsets; see Example 83.
Version 0.4.57 added bounded FoxDot-compatible `PBeat(...)` and `PDur(...)`
duration patterns. Version 0.4.56 added sample-symbol `PShuf(...)`
and `PJoin(...)`: shuffle a complete phrase or join small motifs before shuffling.
Version 0.4.55 added sample-symbol `PStretch(...)`
and `PStutter(...)` transforms; numeric-note forms were added in 0.4.63.
Version 0.4.54 added FoxDot-compatible `PAlt(...)` alternation for sample-symbol
patterns. Version 0.4.53 added FoxDot-style `|` concatenation for finite sample-symbol
`P[...]` patterns, followed by transforms such as `.rotate(n)`. Version 0.4.52
added `.reverse()` and `.rotate(n)` for sample-symbol sequences, while
preserving the existing `sample=` control. Version 0.4.51 added per-hit selectors such as
`|x1|`; 0.4.50 added nested play-string groups: `[]` subdivisions, `()` lacing,
bounded eight-cycle `{}` variations, and adjacent `<>` layers. Sample-symbol
arithmetic remains unsupported and is reported by the compatibility checker. Playback uses the
bundled native SC PlayBuf UGen; no SC/UGen rebuild, libsndfile DLL, external
process or OSC is added. Portable examples 70–92 include six generated CC0
diagnostic WAVs, not a third-party sound library. See
`Portable/Samples/README.txt` for syntax, folder layout and file-format/safety
limits.

The 0.4.46 test branch adds all 26 remaining original FoxDot core-UGen musical
patches (53 FoxDot-derived patches in total), with individual Portable examples
43–68. Full sample-language/library compatibility, audio input and the
SC3-plugin MdaPiano remain separate work.
See `spikes/vst3-callback/foxdot-synths/README.md` for original source attribution,
build-time graph compilation, and the explicit host/safety adaptations.

## MVP decisions

- **Manufacturer:** Dexter U.S.
- **Plugin:** Windows x64 VST3 with a native code editor, SuperCollider audio output, and MIDI event output.
- **Code:** Bundled CPython 3.12 runtime with a deliberately small, FoxDot-style pattern/player API. This is not full FoxDot compatibility.
- **Synthesis:** SuperCollider's synthesis engine runs in the plugin process. No separate SuperCollider application/server process.
- **Control path:** Python evaluates on a worker thread; a bounded event pattern is handed directly to the audio callback. No OSC or external server.
- **Tempo:** Each pattern has its own tempo, defaulting to 120 BPM. Timing is controlled by `tempo(bpm)`, independent of DAW transport.

## Current prototype

- Windows native editor with Run, Stop, Open, Save, and Save As. Starter patterns are separate `.py` files in the `Portable` folder.
- Open and Save As explicitly start in the `Portable` folder beside the VST3 bundle; ordinary Save writes back to the currently opened or saved script.
- The current script is stored in VST3 project state. Restoring a project restores the code but does not execute it automatically.
- `tempo(bpm)`, `scale(name="major", root=60)`, `play(...)`, and eight independently editable players (`p1`–`p8`) using `p1 >> sequence(...)`; `.stop()` stops one player without stopping the others. Pattern helpers include `reverse`, `rotate`, `repeat`, `palindrome`, `stutter`, `interleave`, `transpose`, and `euclid`; finite FoxDot-style `P[...]` note/rest/chord literals and numeric `+`/`-` transposition are supported. Players can choose the four basic oscillator modes plus twenty-one embedded FoxDot-derived synths: bass, varsaw, dirt, jbass, noise, dub, growl, soprano, crunch, lazer, rave, bell, gong, marimba, pluck, twang, donk, squish, charm, spark, and blip. These controls affect plugin audio; MIDI output remains note events.
- Raw MIDI pitches remain the default. With `degrees=True`, zero-based scale degrees map to pitches using major, minor, dorian, mixolydian, major/minor pentatonic, or blues scales; degrees wrap across octaves.
- MIDI note numbers 0–127, `None` rests, repeating patterns, and up to eight simultaneous notes per player (up to 64 across eight players).
- Chords can be written as nested lists, such as `play([[60, 64, 67], [62, 65, 69]])`; separate `play()` calls can layer parts.
- Maximum 256 note events per pattern, 256 step slots, and 4096 beats. Tempo has a 0.001 BPM numerical-safety floor and no fixed upper cap; non-finite values are rejected.
- Embedded CPython runtime and the required SuperCollider UGen modules are packaged with the VST3.
- MIDI note events are emitted on the VST3 event output bus, with each player assigned its own MIDI channel; the engine supports up to 64 simultaneous notes and eight extra short-release voices.
- Internal audio applies MIDI velocity, per-voice attack/release envelopes, and smoothed voice-count mix compensation to avoid abrupt level changes as players start and stop.
- The plugin display name and native editor branding read “Dexter U.S FoxDot”; the VST3 manufacturer metadata remains Dexter U.S.
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
