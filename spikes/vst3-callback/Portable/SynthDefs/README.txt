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

For the loader:
  - `out` is required. The SynthDef may write one mono signal or two stereo
    signals to it with Out.ar(out, signal) or Out.ar(out, [left, right]).
    All Out UGens in a definition must use the same mono or stereo width;
    mixed-width and wider multichannel outputs are rejected. Dexter routes
    either layout to the player. Stereo definitions preserve their center
    image; the player's pan control works as left/right balance.
  - `freq` is optional; when present it receives the note frequency in Hz.
  - `amp`, `pan`, `gate`, `atk`, `rel`, and `sustain` (or `sus`) are optional.
    Attack/release and sustain controls receive seconds. Dexter applies the
    player's amplitude, pan/balance, filters, and final safety
    envelope. If `gate` exists, it is raised at note-on and lowered at the
    note's sustain end. Do not use doneAction: 2; the plugin owns node cleanup.
  - The plugin exposes `userSynth("Name", notes, ...)` to Python. The static
    compatibility checker only accepts names it finds in this folder.

The loader reads each .scsyndef as executable DSP instructions for the
embedded engine. Only install SynthDefs you trust. Check the SynthDef and
any UGen module's license before distributing it.
The folder accepts at most 256 definitions, with an 8 MiB per-file and
64 MiB total size limit.

Custom SynthDef sliders
-----------------------
An optional text sidecar can give up to seven of a custom SynthDef's controls
live Players-panel sliders with explicit, trustworthy ranges. Name it exactly
like the SynthDef but use the `.controls` extension; for example:

  CustomPluck.scsyndef
  CustomPluck.controls

The bundled CustomPluck has `brightness` (pure sine through saw) and `sub`
(sub-octave oscillator blend) sliders. Their defaults are zero, preserving
the earlier sine-only sound. CustomTone provides `blend`, `cutoff`, and
`resonance`.

Each non-comment line in the sidecar is:

  control_name minimum maximum default [linear|log]

Example declarations (the names must actually exist in that SynthDef):

  brightness 0.0 1.0 0.6
  resonance 100 12000 1800 log

Blank lines and `#` comments are allowed. Ranges/defaults are not inferred
from the SynthDef: the sidecar author must declare them. Names must be unique,
Python-style identifiers, and cannot override routing/note controls such as
`out`, `inbus`, `freq`, `amp`, `pan`, `gate`, `atk`, `rel`, or `sus`.
Logarithmic ranges must be positive. Every declared name must match a real
SynthDef parameter; invalid sidecars prevent that SynthDef from loading.
Sidecars are limited to 8 KiB and seven controls. The sliders smoothly update
the running player's graph; these temporary overrides reset on script rerun.
To change a persistent default, edit the sidecar's `default` value.

The included CustomPluck and CustomTone sources and compiled examples are
CC0-1.0:
https://creativecommons.org/publicdomain/zero/1.0/
Example sources for offline compilation: CustomPluck.scd and CustomTone.scd
From the repository root, `python tools/build-portable-user-synthdefs.py`
rebuilds these two optional examples; ordinary Windows VST builds package the
checked-in .scsyndef files and do not compile SuperCollider or UGens.

SCLOrk-derived effects
----------------------
SCLOrkPhazerFX.scsyndef, SCLOrkDecimatorFX.scsyndef, and
SCLOrkCrossoverDistortionFX.scsyndef are stereo player inserts adapted from
SCLOrkSynths by Josh Mitchell. Their .scd source and attribution are included
here; the full GPL-3.0 license is in
../../Licenses/SCLOrkSynths-GPL-3.0.txt.
They use SuperCollider core UGens only; no additional SCPlugins module is
needed. Restart the DAW after copying the .scsyndef files here.
The required packaging/ABI check is tools/test-sclork-synthdefs.py; the
optional native dry/wet audio render is tools/test-sclork-synthdefs-audio.py.
