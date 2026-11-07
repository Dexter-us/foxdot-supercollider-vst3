Jazz brush kit for the Dexter FoxDot VST
========================================

The nine compiled JazzKit*.scsyndef voices are intended to live directly in
Portable/SynthDefs beside the existing SynthDefs. The VST scans this folder
non-recursively, so do not place the .scsyndef files in a nested subfolder.
The editor groups the JazzKit* names under:

    Synths > Drums & Percussion > Jazz Kit

Each matching .controls file exposes bounded, smoothed controls in the Players
panel. Restart the DAW after copying the files; the current engine scans
SynthDefs at startup. This kit uses stock bundled SuperCollider UGens only:
there is no sample library, no additional SCPlugins module, and no OSC server.

The supplied jazz_brush_kit.sclang had no license notice and referenced
brushScrape, brushHat, and brushTap without defining them; it also contained
the malformed amp value 0.3.5. These are newly authored self-contained voices
based on the requested brush-kit sound, rather than a redistributed copy of
that unlicensed source. Confirm licensing before redistributing that original
attachment.

Compile the source once with sclang from this folder:

    sclang JazzKit.sc

The source writes one .scsyndef per voice next to itself. Example 214 uses the
embedded FoxDot-style userSynth() API and the .controls ranges.
