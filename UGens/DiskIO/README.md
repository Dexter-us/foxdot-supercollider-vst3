# Optional DiskIO pack — VST 0.4.80+

This pack contains SuperCollider's `DiskIn`, `VDiskIn`, and `DiskOut` plus a
replaceable shared libsndfile 1.2.2. It does **not** contain or rebuild the SC
engine, other UGen modules, Python, or sclang. Ordinary VST updates do not need
this pack rebuilt or downloaded again while its ABI remains compatible.

## Install and test

Fully close the DAW, back up personal scripts, and **merge** the pack's `Portable`
folder into the existing plugin's `Portable` folder. Do not delete the VST bundle
or its other runtime files. Keep `sndfile.dll` next to `DiskIO_UGens.scx`.
Remove any older duplicate `DiskIO_UGens.scx` from another UGen search folder
after backing it up; only one matching DiskIO module should be installed.
Then restart the DAW and check that the plugin label says 0.4.80 or later.

- Example 106: stereo drum-loop streaming with native DiskIn.
- Example 107: VDiskIn at a different playback speed.
- Example 108: record eight seconds of the post-master stereo plugin output.

The supplied drum loop is generated original CC0 audio, not FoxDot's sample library.

## Python controls

These are explicit **VST extensions**, not the upstream FoxDot `Buffer` API or
an embedded sclang `.ar` graph builder. There is no OSC listener or separate server.
The adapter calls the pinned upstream UGen DSP directly in the host-driven engine.

```python
DiskIn.play("DiskIO/drum-loop.wav", slot=1, loop=True, amp=0.5, pan=0)
DiskIn.stop(slot=1)
VDiskIn.play("DiskIO/drum-loop.wav", slot=1, rate=0.75, loop=True, amp=0.5)
VDiskIn.stop(slot=1)
DiskOut.record(seconds=15)
DiskOut.stop()
```

| Control | Accepted values | Meaning |
|---|---|---|
| `path` | Relative existing audio file under `Portable/Samples` | Absolute paths, `..`, and symlink escapes are rejected |
| `slot` | Integer 1–8 | Independent disk stream, separate from FoxDot p1–p8 |
| `loop` | `True` / `False` | Repeat or play once |
| `amp` | Scalar 0–2 | Disk stream gain; does not prevent clipping when streams are summed |
| `pan` | Scalar −1–1 | Stereo balance; zero preserves source stereo |
| `rate` | Scalar 0.0625–8, VDiskIn only | File-speed multiplier; no reverse or pitch-preserving stretch |
| `seconds` | 0.05–3600, also limited to 2 GiB per WAV | Explicit recording safety bound; default 30 seconds |

Input: mono/stereo files at 8–384 kHz. libsndfile is built without extra codec
libraries or MPEG; WAV/AIFF PCM/float are the first tested formats.
DiskIn advances one source frame per host frame, exactly as SC does, so different
file/host sample rates alter speed/pitch. VDiskIn compensates source/host rate;
its **effective** SC rate must also fit (0, 8].

Re-running a reader command re-cues that stream from the beginning. Streams feed
the master output directly, not a FoxDot player's effect chain. Scalar controls
only in this milestone: `var`/`linvar`, arbitrary `Buffer` operations and arbitrary
disk-based user SynthDef buffer bindings are not claimed supported.

Recordings use exclusive creation of `Portable/Recordings/recording-*.wav`:
float32 stereo, post-master gain, no overwrite. They auto-finalize at the chosen
duration; `DiskOut.stop()`, the plugin Stop button, and normal engine teardown also
flush the last partial buffer and close the file. Recording is not automatically
restarted from saved DAW project state. Keep the host audio engine running while
recording. Disk-full/I/O errors are failures, not successful recordings.

## Why an adapter is necessary

Pinned upstream: SuperCollider `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`,
DiskIO source SHA-256 `37b5f27a58f48950a6ccdd8ce6c1ec6cbaa32aa36eea71ad90a3cecfe655ffe1`.
`prepare-source.py` rejects any other source.

The three DSP functions remain upstream. Adaptations are limited to host/file
management: asynchronous reads even in the VST's non-hardware-driving World;
multi-producer queue for multiple VST instances; a FIFO teardown barrier; finite
short-file looping/error handling; and an explicit recording duration cap.
Stock scsynth's single-producer queue and synchronous NRT reads are unsuitable
for multiple host audio callbacks without these adaptations.

Disk work is on a worker. Audio functions do not open/close files, allocate,
or wait for a disk mutex. A teardown timeout is reported as failure; memory still
referenced by I/O is retained instead of being freed unsafely.

## Build and licenses

```sh
cmake -S UGens/DiskIO -B build/diskio -DSC_SOURCE_DIR=/path/to/pinned/supercollider
cmake --build build/diskio --config Release --parallel 2 --target DiskIO_UGens diskio-tests
# Run diskio-tests with the module path and a writable scratch folder.
```

This compiles **one** UGen module and shared libsndfile, never libscsynth or the
other UGen modules. Windows CI runs the actual UGen DSP and the same disk-controller
class compiled into the VST. Test WAV readback verifies channel data, speed,
short loops, final partial writes, duration cap, unique names, and concurrent Worlds.
These are not a substitute for testing the complete VST in a Windows DAW.

SC and this adapter: GPL-2.0-or-later. libsndfile: LGPL-2.1-or-later (shared and
replaceable). Boost headers: Boost Software License 1.0. Corresponding pinned
source archives, adapter/build sources, and license notices accompany the pack.
