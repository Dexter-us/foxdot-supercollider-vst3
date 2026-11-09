SuperCollider 3.14.1 add-on UGen pack
========================================

Target: Windows x64, SuperCollider release tag Version-3.14.1 (UGen plugin API 3).

This is a separate add-on pack for standalone SuperCollider 3.14.1. It does not replace or rebuild the VST's embedded SuperCollider engine. The VST is pinned to a newer engine/API and cannot load these API-3 modules; this pack is not a VST compatibility fix.

Included additional modules:
- AirwindowsFX.scx: Airwindows Echojam and TapeDelay2 UGens.
- LimutFX.scx: the project Limut effects and vocoder module.

Not included:
- JPverb/Greyhole: you already have the matching reverb pack.
- AntiAliasingOscillators: already included in your official SC3-plugins 3.14.1 download.
- DiskIO_UGens: DiskIO is part of a normal SuperCollider installation; this project also has a VST-host-specific adaptation, whose SC3.14.1 lifecycle test did not pass. Do not install a duplicate DiskIO module from this pack.

Install
------
1. Confirm FoxDot is using SuperCollider 3.14.1. Do not install these modules into the VST or another SuperCollider version.
2. Back up your plugins folder. Copy the two files in SCPlugins into the 3.14.1 plugins folder.
3. Copy the class files in Classes into Platform.userExtensionDir, then restart sclang/FoxDot so the classes compile.
4. SynthDef source and matching graph files are grouped in SynthDefs. The vocoder stays under Sidechain because it needs separate carrier/modulator buses, not a normal one-input effect slot.
5. If sclang rejects any included .scsyndef, compile its matching .scd with the 3.14.1 sclang; these files describe SynthDef graphs, not UGen plugin binaries.

Sources and applicable license/attribution notices are included. The Windows build compiles only AirwindowsFX and LimutFX against the SC 3.14.1 source headers, and runs their checks. It does not build libscsynth, the SuperCollider IDE, or the VST.
