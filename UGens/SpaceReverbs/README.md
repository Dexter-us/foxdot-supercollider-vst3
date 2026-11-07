# Optional JPverb and Greyhole pack

Upstream: `supercollider/sc3-plugins`, immutable commit
`fa926d6b554acef35b2fd9deb2f996085fa86bd7` (Version-3.14.1).
DSP author Julian Parker; fixes Till Bovermann; SC/Faust architecture Stefan
Kersten. The source headers specify GPL-2.0-or-later. The pack includes the
unchanged DSP, its required Faust headers, class/help sources and GPL text.

Only `JPverbRaw.cpp` and `GreyholeRaw.cpp` are built, using the same class/prefix
definitions as upstream. Neither all sc3-plugins nor SuperCollider is compiled.

```sh
python UGens/SpaceReverbs/build-synthdefs.py /path/to/pinned/sc3-plugins
cmake -S UGens/SpaceReverbs -B build-space \
  -DSC_SOURCE_DIR=/path/to/pinned/supercollider \
  -DSC3_PLUGINS_SOURCE_DIR=/path/to/pinned/sc3-plugins
cmake --build build-space --config Release --parallel 2
python UGens/SpaceReverbs/test-examples.py
```

The checked-in precompiled SynthDefs mean installed users do not need sclang.
See `Portable/SPACE-REVERBS-README.txt` for control ranges and installation.
The Windows pack workflow is separate from the cached VST3 workflow. It runs
only for PR changes to this pack/build recipe, or a manual dispatch—not for
ordinary VST source updates.

The distributed pack preserves the source layout under `Sources/sc3-plugins`
and this adapter under `Sources/SpaceReverbs`. To rebuild that pack, use
`cmake -S Sources/SpaceReverbs -B build-space` with the same two directory
options above, pointing `SC3_PLUGINS_SOURCE_DIR` at `Sources/sc3-plugins`.
Obtain SC headers at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`.
For developer NRT audio testing, build a second copy against headers matching
your installed scsynth (its plugin API must match), then run
`python UGens/SpaceReverbs/test-audio.py /path/to/those/modules`.

## Verification before Windows publication

- Both unchanged upstream module sources compile with the pinned SC headers.
- Matching installed SC 3.13 headers/modules pass native NRT audio checks for
  stereo wet tails, repeated input, finite output and exact dry bypass.
- Both SynthDefs pass positional-control/stereo ABI checks. Python control
  endpoints, invalid inputs, var/linvar normalization and stop reset are tested.
- The compiled C++ result parser accepts 162 Portable examples, including
  166-167, and rejects malformed normalized insert-control records.
- Existing Airwindows examples and readiness, gain, sample, DiskIO and note
  effect regressions pass; full processor/runtime C++ syntax checks pass.
- Windows compilation, required VST3 validator and user-DAW listening remain
  pending. No published/validated Windows artifact is claimed by these tests.
