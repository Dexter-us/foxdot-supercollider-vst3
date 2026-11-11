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
Fverb. The pack also includes each module's SuperCollider class and help file.

## Use with the VST

1. Close the DAW completely.
2. Back up the existing `Portable/SCPlugins` folder.
3. Copy the pack's `Portable/SCPlugins/*.scx` files into the existing folder.
   Merge the files; do not delete other plugins already there.
4. Restart the DAW and then start the VST engine.

The VST loads compatible `.scx` modules from `Portable/SCPlugins` at engine
startup. Adding a module does not automatically create a FoxDot synth or
effect: using its UGens in a new sound still requires a SynthDef wrapper and
an example/catalog entry.

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
