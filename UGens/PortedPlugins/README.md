# PortedPlugins (formerly MKPlugins) — Windows x64 pack

This optional pack builds the upstream PortedPlugins collection as standalone
SuperCollider `scsynth` modules for the VST's pinned SuperCollider
3.15.0-dev/API-7 headers. It does not build or replace SuperCollider, the VST,
or Supernova.

Upstream source is pinned to commit
`c6c0d7430e9bc8e8aaeae3db8263e1dbb8f398eb` from
<https://github.com/madskjeldgaard/portedplugins>. The upstream project is
licensed GPL-3.0. DaisySP is fetched at v0.0.2 by the pinned upstream CMake
configuration and is licensed separately. The Windows ZIP includes the
corresponding source archives and license notices.

The build produces 34 `scsynth` modules, including analog bass/snare drums,
oscillators, tape/phaser/distortion, filters, resonators, string voices, and
Fverb. The pack also includes each module's SuperCollider class and help file,
eleven precompiled FoxDot player voices and bounded `.controls` sidecars, plus
ten stereo player-effect inserts. The player voices include Rongs,
PhasorModal, and Resonator. `PortedResonatorFX` exposes live-safe mix,
frequency, structure, brightness, and damping controls; upstream init-only
position/resolution are deliberately fixed. `PortedFverbFX` wraps the pack's
stereo Dattorro-style Fverb and exposes mix, predelay, decay, damping, and
modulation depth. Eight additional inserts wrap AnalogTape, AnalogChew,
AnalogDegrade, AnalogLoss, AnalogPhaser, AnalogVintageDistortion,
LockhartWavefolder, and DCompressor. Their bounded controls appear in the
Effects index and live Players panel. AnalogLoss parameters are smoothed and
should not be rapidly modulated; DCompressor uses its internal detector because
the player chain does not expose an external sidechain bus. Examples 235–242
demonstrate each insert separately, at a conservative level.

## Use with the VST

1. Close the DAW completely.
2. Back up the existing `Portable/SCPlugins` folder.
3. Copy the pack's `Portable/SCPlugins/*.scx` files into the existing folder.
   Merge the files; do not delete other plugins already there.
4. Copy the pack's `Portable/SynthDefs/*.scsyndef` and matching `.controls`
   into the existing VST `Portable/SynthDefs` folder.
5. Restart the DAW, open the VST Players index, and look under Drums &
   Percussion, Plucked & Stringed, and Waveforms.

The VST loads compatible `.scx` modules from `Portable/SCPlugins` at engine
startup. The eleven instrument SynthDef wrappers are selectable as `userSynth`
entries and appear in their categorized Players groups. Other raw modules in
the 34-module set are not automatically playable; additional curated wrappers
are a separate step. The ten insert wrappers are selected through `effect=`
or `effects=`. Examples 232 and 232b cover the player wrappers and octave
checks; Example 233 compares resonator voices; 234 demos Resonator and Fverb;
235–242 isolate the eight new effects.

## Use with standalone SuperCollider/FoxDot

Copy the pack's `Extensions/PortedPlugins` directory into the directory
reported by `Platform.userExtensionDir`, then recompile the SuperCollider
class library and restart the server.

## Reproducible build inputs

- PortedPlugins: `c6c0d7430e9bc8e8aaeae3db8263e1dbb8f398eb`
- SuperCollider headers: `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`
  (SuperCollider 3.15.0-dev, API generation 7)
- DaisySP: v0.0.2, fetched by the pinned PortedPlugins `CMakeLists.txt`
- `SUPERNOVA=OFF`, `SCSYNTH=ON`, `NOVA_SIMD=OFF`
