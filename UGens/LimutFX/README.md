# Optional Limut-inspired effect UGen pack

This first pack adds two effects that do not duplicate the current VST effects:

- `LimutPingPong`: stereo cross-feedback delay with smoothly moving delay time.
- `LimutDuck`: tempo-pump style gain envelope with bounded timing and smoothed
  gate edges.

They are separate SuperCollider plug-in UGens compiled as `LimutFX.scx` on
Windows (or the platform's module suffix elsewhere). The VST3 itself does not
build SuperCollider or these UGens. Install the optional pack once, restart the
DAW, and add its two `.scsyndef` files to `Portable/SynthDefs`.

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
definitions, licensed CC BY-SA 4.0. Attribution and share-alike details are in
`ATTRIBUTION.txt`. The DSP module and adapted examples are distributed under
the same license.
