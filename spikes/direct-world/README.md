# Direct SuperCollider world-render spike

This is the first engine feasibility check for `foxdot-supercollider-vst3`. It loads a tiny `DexterTone` SynthDef into an in-process SuperCollider `World`, creates the graph through SuperCollider's C++ graph APIs, runs audio blocks, and checks the resulting output bus.

## Verified

- SuperCollider source revision: `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10` (3.15.0-dev, Linux x86_64).
- The harness uses `GraphDef_Recv`, `GraphDef_Define`, `World_GetGraphDef`, `Graph_New`, `Group_AddTail`, and `World_Run` directly.
- It produced 4,096 samples at RMS `0.07066852` and peak `0.10000000`; the CTest check passed.
- The harness sends no OSC packets, opens no UDP listener, and does not launch a separate `scsynth` process.

These are internal SuperCollider C++ APIs, not a stable public embedding API. Pin the SuperCollider revision and isolate these calls behind a small adapter.

## Scope and next step

This proves direct, in-process synthesis only. It is **not yet a VST3**, and it does not yet test the DAW audio callback, Windows packaging, Python, MIDI, or transport. The next spike should connect SC audio-bus blocks to a minimal VST3 processor callback while keeping Python, MIDI, editor UI, OSC, and a child server process out of scope.

The harness runs the SC world manually with `mRealTime=false` so it can validate synthesis without opening an audio device. This is deliberately narrower than the VST3 callback test.

## Build and run

Build SuperCollider at the pinned revision, with its submodules initialized. Example Linux configuration:

```sh
cmake -S "$SC_SOURCE_DIR" -B "$SC_BUILD_DIR" -G Ninja \
  -DSC_QT=OFF -DSC_IDE=OFF -DSC_ABLETON_LINK=OFF -DSC_HIDAPI=OFF \
  -DSUPERNOVA=OFF -DNO_LIBSNDFILE=ON -DNO_X11=ON -DFFT_GREEN=ON \
  -DLIBSCSYNTH=OFF -DSTATIC_PLUGINS=OFF \
  -DAUDIOAPI=portaudio -DSYSTEM_PORTAUDIO=OFF
cmake --build "$SC_BUILD_DIR" --target libscsynth OscUGens BinaryOpUGens IOUGens -j1
```

Then build and test this harness:

```sh
cmake -S spikes/direct-world -B build/direct-world -G Ninja \
  -DSC_SOURCE_DIR="$SC_SOURCE_DIR" -DSC_BUILD_DIR="$SC_BUILD_DIR"
cmake --build build/direct-world -j1
ctest --test-dir build/direct-world --output-on-failure
```

`testdata/generate_tone.py` regenerates the small SynthDef asset. Windows and VST3-host validation are still outstanding.

## Licensing note

No SuperCollider source is copied into this repository by this spike. The engine source is GPL-3.0; review the licensing requirements before distributing a plugin that bundles it. See [SuperCollider](https://github.com/supercollider/supercollider).