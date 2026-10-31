Portable user SynthDefs
=======================

Place one precompiled SuperCollider .scsyndef file in this folder for each
custom sound. Its filename (without .scsyndef) must exactly match the
SynthDef's internal name. Names are case-sensitive Python identifiers using
letters, digits, or underscores, with a letter/underscore first; the maximum
length is 255 bytes (SuperCollider's SynthDef name limit).

This VST does not launch sclang or a SuperCollider server. Compile the
SynthDef offline with sclang, then the embedded SuperCollider engine loads
the compiled file when the DAW/plugin starts. A compatible optional UGen
module belongs in Portable/SCPlugins; install that module first and restart
the DAW so the SynthDef can resolve its UGens.

For the first loader version:
  - `out` is required. The SynthDef should write one mono signal to it with
    Out.ar(out, signal). Dexter routes that bus to the player's output.
  - `freq` is optional; when present it receives the note frequency in Hz.
  - `amp`, `pan`, `gate`, `atk`, `rel`, and `sustain` (or `sus`) are optional.
    Attack/release and sustain controls receive seconds. Dexter applies the
    player's amplitude, pan, filters, and final safety
    envelope. If `gate` exists, it is raised at note-on and lowered at the
    note's sustain end. Do not use doneAction: 2; the plugin owns node cleanup.
  - The plugin exposes `userSynth("Name", notes, ...)` to Python. The static
    compatibility checker only accepts names it finds in this folder.

The loader reads each .scsyndef as executable DSP instructions for the
embedded engine. Only install SynthDefs you trust. Check the SynthDef and
any UGen module's license before distributing it.
The folder accepts at most 256 definitions, with an 8 MiB per-file and
64 MiB total size limit.

The included CustomPluck source and compiled example are CC0-1.0:
https://creativecommons.org/publicdomain/zero/1.0/
Example source for offline compilation: CustomPluck.scd
