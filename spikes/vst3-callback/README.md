# Dexter U.S. FoxDot-inspired SuperCollider VST3 prototype

This Windows-first prototype embeds `libscsynth` and CPython 3.12 in a VST3 instrument. Its native editor evaluates a small Python pattern API, then passes bounded note events directly to the VST3 audio callback. The plugin renders up to eight independently running players, with up to eight notes per player (64 total), and emits matching MIDI notes on a separate channel per player. It does not launch `scsynth`, open an audio device, or use OSC.

## What it proves

- The SuperCollider sine voice and a small set of plugin-side oscillator shapes render inside the VST3 module; no server process is launched.
- The host owns the audio callback; each VST block is split into SuperCollider's fixed 64-sample blocks and buffered for variable host block sizes.
- The four required UGen modules (`OscUGens`, `LFUGens`, `BinaryOpUGens`, and `IOUGens`) are copied into `Contents/Resources/SCPlugins`. The plugin locates that folder relative to its loaded module; no build-machine UGen path is compiled into the binary.
- The required CPython embeddable runtime files are copied into `Contents/Resources/Python`, with the Python license notice.
- The editor supports `tempo(bpm)`, `scale(name="major", root=60)`, and `play(...)`, plus eight independent players (`p1`â`p8`) using `p1 >> sequence(...)`; calling `.stop()` stops only that player. It now also accepts an initial FoxDot-style subset directly: `Clock.bpm`, `Scale.default.set(...)`, `p1 >> saw(...)` and `p2 >> pulse(...)`, including cyclic `dur` patterns, scalar `oct`, and `sus` values that fit within their steps. The source text stays unchanged; the embedded Python API maps it to the internal event stream, with no OSC or external server. Pattern helpers include `reverse`, `rotate`, `repeat`, `transpose`, and Euclidean rhythm generation with `euclid(hits, steps, note=60, rotation=0)`. Degree-based playback supports major, minor, dorian, mixolydian, major/minor pentatonic, and blues scales; raw MIDI notes remain the default.
- The Windows editor has visible Open, Save, and Save As controls for UTF-8 Python scripts. Its read-only status field is selectable; click it and press Ctrl+C to copy an error. Nine starter patterns are separate `.py` files in the bundle's `Portable` folder, and Open/Save As explicitly start there rather than using Windows' previously visited directory; ordinary Save writes back to the current script path. The editor prompts before replacing unsaved edits. The footer shows the plugin version.
- VST3 project state stores the editor code. Loading a project restores the script in the editor but does not execute user code automatically.
- Python evaluation is isolated from the real-time callback; the callback reads a fixed-size pattern snapshot and generates audio/MIDI timing from the pattern's own tempo.
- A MIDI output event bus is exposed to the host.

## Prototype limits

- SuperCollider is pinned at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`; the VST3 SDK checkout is 3.8.1. The Windows script bundles CPython 3.12.10. GitHub Actions builds and validates the Windows VST3; audio, editor, and MIDI behavior still require testing in a real Windows DAW.
  - The engine uses `WorldOptions::mRealTime = false` so SuperCollider does not initialize its own audio driver. Seventy-two fixed voice slots support 64 active notes plus eight short release tails. The sine voice uses the embedded SuperCollider SynthDef; triangle, saw, and square are lightweight plugin-side oscillator shapes. This is not a real-time-safety guarantee for arbitrary UGens or future code. Audit allocations, locks, and thread behavior before treating it as a production instrument.
- Audio pitch follows scheduled MIDI notes in 64-sample SuperCollider blocks, so audio onset is quantized to that block size; MIDI events retain sample offsets. Audio uses raised-cosine 10 ms attacks and up to 40 ms releases for natural note ends and explicit stops, plus smoothed voice-count mix adjustment. The envelope duration is unchanged; only the fade curve is smoother.
 - Each player supports up to eight simultaneous notes, and up to eight players can run independently. MIDI is emitted on a separate channel per player. Built-in sine, triangle, saw, and square oscillator choices are available. Each player can set amplitude, stereo pan, low-pass cutoff, and a simple echo; these settings affect internal audio, not emitted MIDI notes. FoxDot sample-string players, the broader synth/effect library, and player aliases beyond the built-in eight players remain deferred; unsupported calls produce explicit Python errors instead of silent fallbacks. There is no arbitrary SynthDef evaluation, and the plugin does not follow DAW transport. The editor is currently Windows-only.
- User code is intentionally constrained but not a security sandbox. Pattern evaluation has a two-second Python loop guard; avoid untrusted scripts.
- SuperCollider is GPL-3.0, the Python runtime uses the PSF license, the VST3 SDK is MIT-licensed, and FoxDot-derived compatibility material is attributed and distributed under CC BY-SA 4.0. Package notices and license texts are included; binary distribution must comply with the relevant source and notice requirements. Review remaining transitive dependency terms before release.

## Build prerequisites

Use a SuperCollider source checkout at the pinned revision, a configured PIC-enabled SC build, the resulting `libscsynth` and UGen modules, the CPython 3.12.10 embeddable runtime, and a VST3 SDK checkout with its required submodules.

Configure the SuperCollider build with `CMAKE_POSITION_INDEPENDENT_CODE=ON`, `LIBSCSYNTH=OFF`, and `STATIC_PLUGINS=OFF`, then build `libscsynth`, `OscUGens`, `LFUGens`, `BinaryOpUGens`, and `IOUGens`. The engine archive must be PIC because it is linked into a shared VST3 module.

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

The build runs Steinberg's validator as a post-build check. `SC_UGEN_PLUGIN_PATH` is used only at build time to find platform-matched modules to copy into the VST3 package; the runtime loads them from the package's `Resources/SCPlugins` folder.

## Windows smoke build

The Windows script and GitHub Actions job use Visual Studio 2022 or 2026 x64 to build the pinned SuperCollider engine and UGen modules, download the pinned CPython embeddable package, build the VST3 bundle, and run the VST3 validator. The script clones the pinned SuperCollider revision and VST3 SDK tag into a temporary build directory; it does not require the full SuperCollider IDE or Qt.

On Windows with Git, CMake 3.25 or newer, and the Visual Studio 2022 or 2026 C++ workload installed, run from the repository root:

```powershell
.\tools\build-windows-vst3.ps1
```

The GitHub Actions workflow also uploads the validated Windows VST3 bundle as a downloadable artifact when the job succeeds. This is a smoke-build path; Windows audio-host behavior still needs to be tested in a real VST3 host.
