# Dexter U.S. FoxDot-inspired SuperCollider VST3 prototype

Version 0.4.49 adds timed sample/rate var() and linvar() controls, preserving the 0.4.48
portable WAV playback and 0.4.47
fresh-note lifecycle correction below. Use `p1 >> play("x-o-", dur=0.5)` with
FoxDot's symbol-folder mapping under `Portable/Samples`. A hyphen selects a
closed-hat sample, while whitespace is a rest. `sample=` and signed `rate=`
accept numbers or P/list patterns; selection and rate cycles expand to their
LCM, with rests consuming parameter steps. They also accept var() and linvar()
for live changes over beat time. Sample selection remains discrete (linvar
switches to the nearest selected WAV at each segment midpoint); playback-rate
linvar glides between same-sign rates, while var() can switch direction.
Rate=1 is natural speed; negative rates reverse the native PlayBuf start and
direction. Playback-rate var/linvar values are evaluated at each trigger; a
single sample voice does not pitch-glide while it is already playing.
Dur controls rhythm; unspecified sus allows natural one-shot length, and
explicit sus crops it.
Amp/pan, player stop/mute, basic effects and MIDI note output remain available.

The standalone C++ reader accepts PCM 8/16/24/32 and IEEE float32 mono/stereo
WAV (including matching extensible GUIDs), at 8–192 kHz. Stereo is currently
downmixed to one mono voice, then panned. Decoding, directory scanning and
allocation stay on the script worker, outside the audio/world lock. Immutable
buffers are published under that lock; both SC buffer mirrors are detached
before world cleanup so SC cannot free plugin-owned vectors. Sample-rate
teardown invalidates the cache generation, preventing stale buffer IDs from
being published. Errors leave active players unchanged.

File limit is 64 MiB, decoded cache budget 128 MiB, rate magnitude 0.0625–8;
existing overlap/polyphony limits still apply. Buffers are retained until
engine reset/unload; reopen after editing an already cached WAV. Nested sample
notation, streaming, loops/slicing, time stretching,
non-WAV codecs and third-party sample-library redistribution remain deferred.
The six tiny diagnostic WAVs are generated locally and offered under CC0.
Examples 70–73 exercise beat symbols, selection, reverse playback and
time-var sample/rate controls. `tools/test-portable-samples.py` compiles the
actual decoder and worker/cache code with undefined-behavior checks, then fills
an SC buffer from decoded floats and checks five native PlayBuf notes, direction/rate,
natural stopping, repeated startup and finite output. It does not use a disk
UGen or soundfile reader; Windows host validation is still required.

Version 0.4.47 fixes fresh-note startup in the new synth bank: pausing a graph
before its first SC render changes Graph_FirstCalc to Graph_NullFirstCalc;
Node_SetRun(1) does not reverse that state until a paused render has occurred.
Creating, pausing and immediately resuming every fresh note therefore left it
silent. Fresh note graphs now stay running and receive their controls before
the first World_Run. New-bank paused templates are no longer preallocated.
The private-bus/numeric-control audio regression reproduces the old silence
with --paused-start and passes all 26 graphs with the corrected lifecycle.
Example 69 adds an arranged pattern-composition/star/karp demo with a quiet sine
reference, rather than repeating the isolated four-note test.

It also fixes startup reporting and recovery: Run retries an unavailable
engine once on the script worker after the host has configured audio, rather
than returning the same generic "not ready" message indefinitely. Before host
audio setup it gives explicit host-processing instructions and does not guess
a sample rate. Startup failure stage/details are shown in the copyable editor
status, and successful startup clears stale errors. Teardown, sample-rate
changes and initialization share the same lock. The readiness regression test
uses the actual entry logic with a fake backend; it is not an audio/DAW test.
The lifecycle fix passes offline audio tests; confirmation in the user's
Windows host is still needed.

Version 0.4.46 adds all 26 remaining core-UGen FoxDot musical patches in one
batch: viola, nylon, arpy, dab, sawbass, dbass, prophet, pads, pasha, ambi,
sinepad, keys, klank, feel, glass, soft, quin, space, scratch, creep, snick,
swell, razz, sitar, star, and karp. Portable examples 43–68 test each one
individually. The Python checker, editor highlighting/completion, runtime
selector, and C++ definition registry cover the entire batch. The synth named
`swell(...)` is separate from the existing `swell=` effect control.

The checked-in bank is compiled by sclang from vendored, pinned original
FoxDot `.scd` files, not hand-built oscillator approximations. See
`foxdot-synths/README.md` for the trigger/routing and safety adaptations.
Windows builds still use cached SC outputs and do not compile UGens.
MdaPiano/Decimator, samples, and audio input are not added.

This Windows-first prototype embeds `libscsynth` and CPython 3.12 in a VST3 instrument. Its native editor evaluates a small Python pattern API, then passes bounded note events directly to the VST3 audio callback. The plugin renders up to eight independently running players, with up to eight notes per player (64 total), and emits matching MIDI notes on a separate channel per player. It does not launch `scsynth`, open an audio device, or use OSC.

## What it proves

- Sine, triangle, saw, and pulse/square voices are SynthDefs rendered by the embedded SuperCollider UGens; no server process is launched.
- Version 0.4.43 embeds twenty-seven FoxDot-derived SynthDefs—`bass`, `varsaw`, `dirt`, `jbass`, `noise`, `dub`, `growl`, `soprano`, `crunch`, `lazer`, `rave`, `bell`, `gong`, `marimba`, `pluck`, `twang`, `donk`, `squish`, `charm`, `spark`, `blip`, `scatter`, `orient`, `zap`, `fuzz`, `bug`, and `ripple`—based on [FoxDot’s `_SynthDefs.py`](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/SCLang/_SynthDefs.py), licensed CC BY-SA 4.0. Their graphs use bundled SuperCollider UGens; no extra runtime libraries are introduced. Bass and varsaw use FoxDot's source-style envelopes and amp scaling when the default `atk`/`rel` values are left untouched; charm, spark, and blip use their source-style amplitude motion. Examples 31–42 exercise the synth bank. The synth-to-graph index map now covers the whole bank, including `scatter` through `ripple`.
- Startup registers the synth definitions but no longer constructs every possible synth graph for every voice. A player’s selected synth graphs are created on the script worker as needed, and each voice keeps a bounded cache of two graph types. The audio callback never blocks on graph preparation; callbacks overlapping a non-realtime graph update output silence rather than waiting. Voice slots are partitioned by player, with eight-note polyphony plus room for overlapping release tails.
- The 0.4.39 Charm graph correction pairs each sine oscillator with its matching VarSaw and filter path. The 0.4.40 first self-contained batch adds scatter, orient, zap, fuzz, bug, and ripple using standard SuperCollider UGens.
- The VST continues to manage MIDI note-on/off independently of these audio envelopes. The reverb send remains alive for a short silent tail after an audible voice ends, so its control-rate fade completes before its SuperCollider graph stops.
- The host owns the audio callback; each VST block is split into SuperCollider's fixed 64-sample blocks and buffered for variable host block sizes.
- The plugin bundles 26 official SuperCollider UGen modules in `Contents/Resources/SCPlugins` and also scans the portable `Portable/SCPlugins` folder for additional compatible Windows `.scx` modules at startup. The bundle includes `DiskIO_UGens` (`DiskIn`, `DiskOut`, and `VDiskIn`) plus the libsndfile 1.2.2 runtime DLL; its LGPL-2.1 license is included. `UIUGens` is included too: `MouseX`, `MouseY`, and `MouseButton` read global desktop input, while `KeyState` is registered but keyboard polling is disabled in SuperCollider's Windows implementation. No sample assets are included, and the Python API does not yet provide FoxDot sample players or buffer-file controls. Restart the DAW after adding modules. Ableton Link and Bela-specific modules remain excluded. The bundled SuperCollider modules use GPL terms; Boost headers used at build time are covered by the included Boost Software License notice.
- The required CPython embeddable runtime files are copied into `Contents/Resources/Python`, with the Python license notice.
- The editor supports `tempo(bpm)`, `scale(name="major", root=60)`, and `play(...)`, plus eight independent players (`p1`-`p8`) using `p1 >> sequence(...)`; calling `.stop()` stops only that player, while `.mute()` and `.unmute()` silence or restore a layer without stopping its pattern. Rerunning a script updates only the player slots named in that run; unmentioned players keep playing, as shown in Example 17. It now also accepts an initial FoxDot-style subset directly: `Clock.bpm`, `Scale.default.set(...)`, `p1 >> saw(...)` and `p2 >> pulse(...)`, including cyclic `dur` patterns, scalar `oct`, and `sus` values that fit within their steps. The source text stays unchanged; the embedded Python API maps it to the internal event stream, with no OSC or external server. Pattern helpers include `reverse`, `rotate`, `repeat`, `palindrome`, `stutter`, `interleave`, `transpose`, and Euclidean rhythm generation with `euclid(hits, steps, note=60, rotation=0)`. Finite `P[0, 2, None, (3, 5)]` literals convert to note lists with rests and chords; `+` and `-` support numeric transposition and FoxDot-style cyclic pattern lacing. Finite P patterns also support FoxDot-style `.reverse()`, `.rotate()`, `.stutter()`, `.palindrome()`, plus whole-pattern `.repeat()`. Slice-based and dynamic FoxDot patterns and sample strings remain unsupported. Example 10 demonstrates helper transformations, Example 11 demonstrates `P[...]` notation, Example 12 demonstrates `P` arithmetic, and Example 13 demonstrates transformation methods on P patterns, and Example 14 demonstrates `|` phrase concatenation and `&` pattern zipping into chords; Example 15 demonstrates the additional `*`, `/`, `//`, `%`, and `**` arithmetic operators; Example 16 demonstrates bounded `P` ranges and composable slices, and Example 17 shows how to update one player without stopping the others. Example 23 demonstrates FoxDot-compatible FreeVerb plus optional FreeVerb2 and GVerb reverb algorithms. Example 24 adds per-player resonant LPF/HPF/BPF filters, `bpnoise` modulation and the envelope-driven `swell` high-pass effect; Example 37 isolates both modulation effects. Example 25 demonstrates FoxDot-style beat-based `echo` spacing and `decay`; Example 22 combines them with beat-offset `delay` and the separate plugin extensions `delaytime`, `echomix`, and `feedback`. Degree-based playback supports major, minor, dorian, mixolydian, major/minor pentatonic, and blues scales; raw MIDI notes remain the default.
- The scale bank now includes 44 equal-tempered FoxDot scale names plus custom ascending semitone scales. `Scale.default = ...`, `Scale.default.set(...)`, `Root.default.set(...)`, and per-player `root=` support key and scale selection; Example 26 covers scale/chord groups, Example 27 tests global and per-player roots, Example 28 demonstrates FoxDot's Roman-numeral triads/seventh chords and `PGroup(...)`, and Example 29 demonstrates a gentler two-layer per-note chord strum, and Example 30 demonstrates player mute/unmute while patterns keep running. Just-intonation/frequency scales remain unsupported.
- The Windows editor has visible Run, Stop, Open, Save, and Save As controls for UTF-8 Python scripts. Run performs a static compatibility check first; known unsupported FoxDot calls, player aliases, controls, Clock/Scale/Root attributes, imports, and sample strings are reported with line numbers, playback does not start until the reported issues are addressed, and the Copy Error button copies the full report. A clean static check then runs the script. The check does not execute code and cannot guarantee that runtime-computed values will succeed. The Copy Error button copies complete runtime exceptions and compatibility reports, including details that do not fit in the status line. Runtime checks distinguish repeated-pitch overlap from the eight-note polyphony limit and identify the player, MIDI pitch, and beat. Thirty-seven starter patterns are separate `.py` files in the bundle's `Portable` folder, and Open/Save As explicitly start there rather than using Windows' previously visited directory; ordinary Save writes back to the current script path. The editor prompts before replacing unsaved edits. The footer shows the plugin version, and a filename label identifies the loaded script.
- VST3 project state stores the editor code. Loading a project restores the script in the editor but does not execute user code automatically.
- Python evaluation is isolated from the real-time callback; the callback reads a fixed-size pattern snapshot and generates audio/MIDI timing from the pattern's own tempo.
- A MIDI output event bus is exposed to the host.

## Prototype limits

- Version 0.4.45 rounds both SuperCollider hash-table capacities up to powers of two. The previous graph-definition capacity of 31 violated the pinned hash implementation's bit-mask probing requirement. Compile-time guards and a bounded probing regression test cover this constraint. Lifecycle timing traces remain enabled outside the audio callback; Windows CI preserves validator logs and treats timeout or failure as a workflow failure. Resolution of the host freeze still requires runtime verification.

- SuperCollider is pinned at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`; the VST3 SDK checkout is 3.8.1. The Windows script bundles CPython 3.12.10. GitHub Actions builds and validates the Windows VST3; audio, editor, and MIDI behavior still require testing in a real Windows DAW.
  - The engine uses `WorldOptions::mRealTime = false` so SuperCollider does not initialize its own audio driver. One hundred thirty-six fixed voice slots provide 17 slots per player for active notes and overlapping release tails. All four voice modes (sine, triangle, saw, and pulse/square) use the embedded SuperCollider SynthDef and bundled UGen modules. This is not a real-time-safety guarantee for every UGen or third-party module; only load trusted, compatible modules and test them in a DAW. Audit allocations, locks, and thread behavior before treating it as a production instrument.
- Audio pitch follows scheduled MIDI notes in 64-sample SuperCollider blocks, so audio onset is quantized to that block size; MIDI events retain sample offsets. Audio uses raised-cosine 10 ms attacks and up to 40 ms releases for natural note ends and explicit stops, plus smoothed voice-count mix adjustment. The envelope duration is unchanged; only the fade curve is smoother.
- Each player supports up to eight simultaneous notes, and up to eight players can run independently. MIDI is emitted on a separate channel per player. The four starter oscillator modes and twenty-seven embedded FoxDot-derived patches are available. Each player can set amplitude, stereo pan, low-pass cutoff, echo, and reverb; these settings affect internal audio, not emitted MIDI notes. FoxDot-style `strum` staggers successive notes of a chord by a beat-based interval, and may use `var`/`linvar`. Player mute fades audio smoothly and sends MIDI note-offs on mute; unmute resumes the unchanged pattern on its next notes. FoxDot-style `delay` offsets note starts in beats, and `echo` spacing plus `decay` tail duration are beat-based. Plugin-only `delaytime` (seconds override), `echomix` (wet level), and `feedback` (manual feedback) remain distinct extensions. FoxDot-compatible room/mix uses FreeVerb, while the per-player reverb selector also offers FreeVerb2 and GVerb; GVerb exposes revtime and spread as extensions. The DiskIO module is available to SuperCollider SynthDefs, but the current Python editor does not expose sample-player syntax or buffer-file controls; FoxDot sample-string players and player aliases beyond the built-in eight players remain deferred. Unsupported calls produce explicit Python errors instead of silent fallbacks. Run performs a static compatibility report before playback; it does not execute the script and cannot guarantee values computed dynamically at runtime. There is no arbitrary SynthDef evaluation, and the plugin does not follow DAW transport. The editor is currently Windows-only.
- User code is intentionally constrained but not a security sandbox. Pattern evaluation has a two-second Python loop guard; avoid untrusted scripts.
- SuperCollider is GPL-3.0, the Python runtime uses the PSF license, the VST3 SDK is MIT-licensed, and FoxDot-derived compatibility material is attributed and distributed under CC BY-SA 4.0. Package notices and license texts are included; binary distribution must comply with the relevant source and notice requirements. Review remaining transitive dependency terms before release.

## Build prerequisites

Use a SuperCollider source checkout at the pinned revision, a configured PIC-enabled SC build, the resulting `libscsynth` and UGen modules, the CPython 3.12.10 embeddable runtime, and a VST3 SDK checkout with its required submodules.

The Windows build script downloads the libsndfile 1.2.2 x64 package and verifies its SHA-256 before use. Configure SuperCollider with `CMAKE_POSITION_INDEPENDENT_CODE=ON`, `LIBSCSYNTH=OFF`, `STATIC_PLUGINS=OFF`, and `NO_LIBSNDFILE=OFF`, then build `libscsynth` and the 26 supported UGen modules listed by the build script. The Windows build enables `UIUGens` without requiring X11. The engine archive must be PIC because it is linked into a shared VST3 module.

```sh
cmake -S spikes/vst3-callback -B build/vst3-callback -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVST3_SDK_ROOT=/path/to/vst3sdk \
  -DSC_SOURCE_DIR=/path/to/supercollider \
  -DSC_BUILD_DIR=/path/to/supercollider-build \
  -DSC_UGEN_PLUGIN_PATH=/path/to/supercollider-build/server/plugins \
  -DPYTHON_RUNTIME_DIR=/path/to/python-3.12-embed
cmake --build build/vst3-callback --target dexter_sc_tone
```

The build runs Steinberg's validator as a post-build check. `SC_UGEN_PLUGIN_PATH` is used only at build time to find platform-matched bundled modules. At runtime, the embedded SuperCollider loader searches `Contents/Resources/SCPlugins` and `Portable/SCPlugins`; the latter is the extension folder for user-added modules.

## Windows smoke build

The Windows script and GitHub Actions job use Visual Studio 2022 or 2026 x64 to build the pinned SuperCollider engine and UGen modules, download the pinned libsndfile and CPython embeddable packages, build the VST3 bundle, and run the VST3 validator. The script clones the pinned SuperCollider revision and VST3 SDK tag into a temporary build directory; it does not require the full SuperCollider IDE or Qt.

On Windows with Git, CMake 3.25 or newer, and the Visual Studio 2022 or 2026 C++ workload installed, run from the repository root:

```powershell
.\tools\build-windows-vst3.ps1
```

The GitHub Actions workflow also uploads the validated Windows VST3 bundle as a downloadable artifact when the job succeeds. This is a smoke-build path; Windows audio-host behavior still needs to be tested in a real VST3 host.
