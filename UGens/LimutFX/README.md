# Optional Limut-inspired effect UGen pack

The pack contains four optional stereo inserts:

- `LimutPingPong`: stereo cross-feedback delay with smoothly moving delay time.
- `LimutDuck`: tempo-pump style gain envelope with bounded timing and smoothed
  gate edges.
- `LimutDisperser`: a 1–24-stage all-pass phase disperser with smoothed
  frequency, Q, and stage-count changes.
- `LimutFuzz`: a bounded soft-clipping drive stage with a smoothed post-drive
  tone cutoff.

They are separate SuperCollider plug-in UGens compiled as `LimutFX.scx` on
Windows (or the platform's module suffix elsewhere). The VST3 itself does not
build SuperCollider or these UGens. Install the optional pack once, restart the
DAW, and add the matching `.scsyndef` files to `Portable/SynthDefs`.

Controls are `fx_*` insert parameters: Disperser mix 0–1, frequency 30–18000 Hz,
Q 0.1–10, stages 1–24; Fuzz mix 0–1, drive 0–36 dB, tone 200–16000 Hz.
Frequency, Q, stage count, and tone are smoothed; values are clamped to safe
ranges. The Disperser uses cascaded digital all-pass sections, while Fuzz uses
normalized tanh saturation followed by a one-pole tone filter. These are
standalone DSP implementations, not bit-identical SuperCollider or Limut
graphs. Example 196 demonstrates both inserts and their order in a chain.

The implementation is intentionally incremental. Limut also defines effects
that already overlap this VST (chorus, flanger, phaser, echo, tape and reverb),
as well as larger granular, vocoder, multiband and fuzz graphs. Those are not
duplicated in this first pack; they can be mapped to existing effects or ported
as later, separately tested modules.

## Build and test

```sh
cmake -S UGens/LimutFX -B build-limutfx \
  -DSC_SOURCE_DIR=/path/to/pinned/supercollider
cmake --build build-limutfx --config Release --parallel 2
ctest --test-dir build-limutfx --output-on-failure
python UGens/LimutFX/test-examples.py
```

To regenerate the checked-in SynthDefs with `sclang`:

```sh
python UGens/LimutFX/build-synthdefs.py
```

The SuperCollider headers are used to compile only this optional module; the
SuperCollider engine and existing VST/UGen packs are not rebuilt.

## License and attribution

The PingPong and Duck adaptations are based on Limut's `effects.limut`
definitions, licensed CC BY-SA 4.0. The Disperser and Fuzz DSP are separate
implementations authored for this pack. Attribution and license details are in
`ATTRIBUTION.txt`; the pack is distributed under CC BY-SA 4.0.
