# Direct SuperCollider-in-VST3 callback spike

This prototype is a VST3 instrument that renders a fixed SuperCollider 440 Hz test SynthDef. It embeds `libscsynth` into the VST3 module, calls `World_Run` from the VST3 `process()` callback, and does not launch a separate `scsynth` process, open an audio device, or use OSC. It is not FoxDot or a user-editable live-coding interface yet.

## What it proves

- A SuperCollider world and SynthDef can render inside a VST3 module.
- The host owns the audio callback; each VST block is split into SuperCollider's fixed 64-sample blocks and buffered for variable host block sizes.
- The module passes the Steinberg VST3 validator on Linux: 47 tests passed, 0 failed.
- A separate direct-world CTest harness also confirmed nonzero audio output (RMS 0.0707, peak 0.1).

## Prototype limits

- The tested SuperCollider revision is pinned at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10` (3.15.0-dev), and the VST3 SDK checkout is 3.8.1. Windows has not been built or tested.
- The engine uses `WorldOptions::mRealTime = false` so SuperCollider does not initialize its own audio driver. This one-voice test graph runs in the host callback, but this is not a real-time-safety guarantee for arbitrary UGens or future code. Audit allocations, locks, and thread behavior before treating it as a production instrument.
- The required UGen modules are loaded from `SC_UGEN_PLUGIN_PATH`; they are not yet copied into the VST3 bundle. So this build is not portable/self-contained.
- The prototype contains only a fixed test tone. It has no editor, Python/FoxDot, MIDI input/output, tempo controls, host transport, presets, or code execution UI.
- SuperCollider is GPL-licensed. Review licensing obligations before distributing a binary that statically links it.

## Build prerequisites

Use a SuperCollider source checkout at the pinned revision, a configured PIC-enabled SC build, the resulting `libscsynth` and UGen modules, and a VST3 SDK checkout with its required submodules.

Configure the SuperCollider build with `CMAKE_POSITION_INDEPENDENT_CODE=ON`, `LIBSCSYNTH=OFF`, and `STATIC_PLUGINS=OFF`, then build `libscsynth`, `OscUGens`, `BinaryOpUGens`, and `IOUGens`. The engine archive must be PIC because it is linked into a shared VST3 module.

```sh
cmake -S spikes/vst3-callback -B build/vst3-callback -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVST3_SDK_ROOT=/path/to/vst3sdk \
  -DSC_SOURCE_DIR=/path/to/supercollider \
  -DSC_BUILD_DIR=/path/to/supercollider-build \
  -DSC_UGEN_PLUGIN_PATH=/path/to/supercollider-build/server/plugins
cmake --build build/vst3-callback --target dexter_sc_tone
```

The build runs Steinberg's validator as a post-build check. The UGen path is compiled into this prototype as an absolute development path; bundle-relative loading is a later packaging task.
