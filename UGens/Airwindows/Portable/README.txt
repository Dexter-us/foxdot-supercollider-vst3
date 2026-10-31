Dexter U.S. FoxDot — Airwindows UGen add-on pack

Contents
--------
SCPlugins/AirwindowsFX.scx
SynthDefs/AirwindowsTapeDelay2.scsyndef
SynthDefs/AirwindowsEchojam.scsyndef
SynthDefs/*.sc and *.scd source
95-airwindows-tape-delay2.py
96-airwindows-echojam.py
97-airwindows-effects-together.py
../LICENSE-Airwindows.txt

Install by merging this pack's Portable folder into the Portable folder beside
the VST3 bundle. Keep the existing Python runtime, SCPlugins, SynthDefs, and
your own scripts; copy/merge these files rather than replacing those folders.
Restart the DAW after installing the .scx module. Then load Examples 95 or 96
from the plugin's Portable folder. Example 97 shows both effects on separate
players at the same time.

The DSP is Airwindows' original TapeDelay2 and Echojam float processor code,
pinned to commit 7fd5f5cae262a41e6dd7d9a83d60c46e84d3092a. The C++ adapter
changes the host interface only; it preserves the original processing methods
and parameter curves. See LICENSE-Airwindows.txt and the source files in the
pack's Sources folder.

The modules have no runtime third-party dependencies, server, OSC, or disk
access. Each UGen is stereo; the current VST's per-note voice bus is mono, so
the included SynthDefs downmix its two outputs before the host applies player
panning. Custom SuperCollider SynthDefs can use both UGen outputs directly.

The large delay state belongs to each SynthDef voice, not to the effect as a
shared insert. Example 97 intentionally keeps each player monophonic, so both
effects can run concurrently without multiplying their large buffers across
chords. The VST does not run sclang;
the included .scsyndef files are precompiled. sclang is needed only if you
want to rebuild those SynthDefs after editing their .scd source.

In these two SynthDefs, the existing FoxDot-style controls are mapped as:
TapeDelay2: vib=time, depth=regen, lforate=tone, lfowidth=resonance,
rq=flutter, blur=dry/wet.
Echojam: vib=delay-layout selector, depth=regen, lforate=speed, blur=dry/wet.
The Airwindows parameters are normalized and clipped to 0..1.
