Dexter U.S. FoxDot — Airwindows UGen add-on pack
Compatible with VST3 version 0.4.189 and later

Contents
--------
SCPlugins/AirwindowsFX.scx
SynthDefs/AirwindowsTapeDelay2.scsyndef
SynthDefs/AirwindowsEchojam.scsyndef
SynthDefs/*.sc and *.scd source
95-airwindows-tape-delay2.py
96-airwindows-echojam.py
97-airwindows-effects-together.py
98-airwindows-chord-tails.py
99-echojam-drum-delay-demo.py
../LICENSE-Airwindows.txt

Install by merging this pack's Portable folder into the Portable folder beside
the VST3 bundle. Keep the existing Python runtime, SCPlugins, SynthDefs, and
your own scripts; copy/merge these files rather than replacing those folders.
Restart the DAW after installing the .scx module. Load Examples 95–99 from the
plugin's Portable folder. Example 97 shows both effects at once; Example 98
demonstrates chord input and persistent tails. In VST version 0.4.189 or later,
Example 229 chooses between the two delays per note with seeded `PRand`.

The DSP is Airwindows' original TapeDelay2 and Echojam float processor code,
pinned to commit 7fd5f5cae262a41e6dd7d9a83d60c46e84d3092a. The C++ adapter
changes the host interface only; it preserves the original processing methods
and parameter curves. See LICENSE-Airwindows.txt and the source files in the
pack's Sources folder.

The modules have no runtime third-party dependencies, server, OSC, or disk
access. The VST routes each player's summed stereo signal to one persistent
insert graph and returns the original stereo UGen output. This preserves the
effect's delay memory across notes and chords; the state is allocated once per
player, not once per note. The plugin reserves a 64 MiB SuperCollider real-time
pool per instance, which accommodates all eight player inserts.

The VST does not run sclang;
the included .scsyndef files are precompiled. sclang is needed only if you
want to rebuild those SynthDefs after editing their .scd source.

The 0.4.189 SynthDefs add a smoothed outer `fx_mix` dry/wet control (0–1,
default 1, preserving the previous full-effect sound). The existing Airwindows
parameter named `mix` remains available as `blur`; it is separate from `fx_mix`.
The FoxDot-style controls are mapped as:
TapeDelay2: vib=time, depth=regen, lforate=tone, lfowidth=resonance,
rq=flutter, blur=internal mix.
Echojam: vib=delay-layout selector, depth=regen, lforate=speed, blur=internal
mix.
The Airwindows parameters are normalized and clipped to 0..1.
