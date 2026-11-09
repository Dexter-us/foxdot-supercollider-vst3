SuperCollider 3.14.1 add-on UGen pack
========================================

Target: Windows x64, SuperCollider release tag Version-3.14.1 (UGen plugin API 3).

This is a separate add-on for a standalone SuperCollider 3.14.1 installation. It does not replace or rebuild the VST's embedded SuperCollider engine. The VST remains pinned to a newer engine/API and cannot load these API-3 modules; this pack is not a VST compatibility fix.

The Windows workflow compiles only these add-on modules against the 3.14.1 source headers:
- AirwindowsFX.scx (Echojam and TapeDelay2)
- LimutFX.scx (the project Limut effects/vocoder module)
- DiskIO_UGens.scx with sndfile.dll (DiskIn, VDiskIn, DiskOut)

JPverb and Greyhole are intentionally omitted because you already have the matching reverb pack. AntiAliasingOscillators is also omitted because it is already included in your official SC3-plugins 3.14.1 download.

Installation
------------
1. Confirm FoxDot is using SuperCollider 3.14.1. Do not install these modules into the VST or another SuperCollider version.
2. Back up the existing plugins folder. Copy AirwindowsFX.scx and LimutFX.scx into the 3.14.1 plugins folder. Install their matching class files from Classes into Platform.userExtensionDir, then restart sclang/FoxDot.
3. DiskIO_UGens.scx is optional: standard SuperCollider installations may already contain it. Do not install a duplicate. If it is absent and you choose to use this pack's copy, keep sndfile.dll beside it.
4. The matching SynthDef source examples are in SynthDefSource; compile/load them with the 3.14.1 sclang after the UGen classes are visible.

The package includes the corresponding project/upstream source and license notices. The build compiles the three modules and their tests only; it does not build libscsynth, the SuperCollider IDE, or the VST.
