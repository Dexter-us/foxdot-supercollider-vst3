# Optional Limut-inspired effect UGen pack

The pack contains eight optional stereo inserts plus one two-input UGen:

- `LimutPingPong`: stereo cross-feedback delay with smoothly moving delay time.
- `LimutDuck`: tempo-pump style gain envelope with bounded timing and smoothed
  gate edges.
- `LimutDisperser`: a 1–24-stage all-pass phase disperser with smoothed
  frequency, Q, and stage-count changes.
- `LimutFuzz`: a bounded soft-clipping drive stage with a smoothed post-drive
  tone cutoff.
- `LimutShifter`: a windowed two-head pitch shifter, with ratio 0.5–2 and
  window 0.02–0.5 seconds.
- `LimutShimmer`: a pitch-shifted signal into a stereo feedback reverb; ratio
  0.5–2, window 0.02–0.5 seconds, room and damping 0–1.
- `LimutGatedVerb`: a stereo reverb with a smoothed threshold gate; threshold
  0.001–0.25, hold 0–2 seconds, attack 0.001–0.25 seconds and release
  0.01–1 second.
- `LimutCompressor`: a stereo-linked compressor with dB threshold, Limut-style
  ratio-in-dB, attack/release and soft knee controls.
- `LimutVocoder`: a 16-band carrier/modulator vocoder UGen. It is compiled into
  `LimutFX.scx`, but the current embedded player-effect chain has no second
  modulator bus, so it is intentionally not offered as `effect=` yet.

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
graphs. Shifter, Shimmer, GatedVerb and Compressor are standalone bounded DSP
implementations; they use no additional libraries or SuperCollider plugin
modules beyond this `LimutFX.scx`. Examples 197–199 demonstrate the inserts,
including a Shifter → Shimmer chain and the compressor. The standalone
two-bus SuperCollider vocoder example is in `Portable/Sidechain`.

Limut also defines effects that overlap this VST (chorus, flanger, phaser, echo,
tape and reverb); these remain represented by the existing effect controls.
Granular and multiband processing are not included in this batch.

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

PingPong and Duck behavior, the Compressor control model and the Vocoder node
topology are based on Limut material licensed CC BY-SA 4.0. Attribution and
license details are in `ATTRIBUTION.txt`; the pack is distributed under
CC BY-SA 4.0.
