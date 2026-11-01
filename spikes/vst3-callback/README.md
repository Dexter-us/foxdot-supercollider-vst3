# Dexter U.S. FoxDot-inspired SuperCollider VST3 prototype

Version 0.4.85 adds FoxDot-style `cycle=` phase offsets and `ident=` schedule
keys to `.every()`, `.often()`, `.sometimes()`, and `.rarely()`. Different
identifiers let the same operation run as independent schedules; repeating a
command with the same identifier replaces that schedule, while an omitted
identifier preserves FoxDot's default replace-by-command behavior. `.every()`
uses a scalar phase within a scalar cycle. The random shortcuts can also use a
cycle, compiled as a seeded four-phase repeating phrase rather than an
unbounded random stream. The command/identifier scheduling key follows
FoxDot's [`Repeatable.every`](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/Repeat.py)
behavior. Sample players may run multiple identified stutter schedules; other
sample transforms remain unsupported. All schedules are bounded to 256 calls
and 256 output events. Portable Example 114 demonstrates cycle/identifier
scheduling.

Version 0.4.84 lets numeric players combine distinct scheduled operations, for
example `.often("shuffle")` with `.every(1, "stutter", 3)`, as shown in
Portable Example 113. The supported operations are `reverse`, `rotate`,
`shuffle`, and `stutter`.

Version 0.4.83 adds scheduled stutter for WAV drum players, as shown in
Portable Example 112. A stutter repeats a sample hit only when the scheduled
beat matches a hit onset; rest beats are skipped. It preserves the sample
selection and evenly spaces the retriggers before the next hit. Sample stutter
requires unstrummed hits without per-note delay; scheduled reverse/rotate/shuffle
remain numeric-player operations.

Version 0.4.82 adds FoxDot-style `.often()`/`.rarely()` scheduling shortcuts and
scheduled `shuffle`/`stutter` operations alongside `reverse`/`rotate`. The
upstream 0.6.7 interval ranges are used: often 0.5–4 beats, sometimes 4–16,
rarely 16–32; the final interval may be nudged within the same range to close a
bounded repeating phrase. Schedules are seeded, and Python does not remain
running as a server. Stutter retriggers the current note within its remaining
step and accepts counts 1–16. See Portable Example 111.

Version 0.4.81 adds numeric player degree references (`p2.degree = p1.degree`
and `p2.follow(p1)`) plus bounded player `.every()`/`.sometimes()` transformations.
Degree changes propagate to followers while each player keeps its own synth,
timing, and mix controls. See Portable Examples 109–110. This embedded subset
requires dense single-note numeric patterns and does not combine scheduled
operations with rests, chords, strum/delay, variable octave, or player-follow
relationships on the same player. Degree transforms leave other controls fixed.

Version 0.4.80 adds an optional standalone DiskIO pack: native DiskIn/VDiskIn
streaming and bounded DiskOut master recording. Examples 106–108 use explicit
VST extensions, not an embedded sclang interpreter or the full FoxDot Buffer API.
Recordings are unique float32 stereo WAVs in `Portable/Recordings`.
See `UGens/DiskIO/README.md`. Ordinary plugin builds still disable libsndfile and
reuse the SC cache; the pack builds only one UGen module and shared libsndfile.

Version 0.4.79 adds beat-synced `var()` and `linvar()` modulation to the
per-player Airwindows insert controls. `var()` changes are smoothed over a short
15 ms time constant; `linvar()` follows the beat clock, with controls updated
once per SuperCollider block. See Portable Example 105.

Version 0.4.78 exposed per-player Airwindows insert controls on sample `play()`.
Use fixed 0–1 scalar arguments `fx_vib`, `fx_depth`, `fx_lforate`, and `fx_blur` with either
`effect="AirwindowsTapeDelay2"` or `effect="AirwindowsEchojam"`; TapeDelay2 also
accepts `fx_lfowidth` and `fx_rq`. In 0.4.78 controls are fixed scalars from 0
to 1 and take effect when the script is evaluated. Example 103 demonstrates all six
TapeDelay2 settings and Example 104 demonstrates Echojam's four controls, both
while preserving sample sequence and slice controls. The matching SynthDef and
UGen pack are still required.
Version 0.4.76 adds bounded FoxDot-style `PRand` and `PWhite` pattern
generators. `PRand` chooses from a finite pattern or an inclusive integer
range; `PWhite` follows FoxDot's symmetric triangular distribution. Both
generate finite, seeded sequences of 1–256 items (default 256), so they can
run without imports or an external server; Example 102 demonstrates both.
The separate Airwindows UGen-pack workflow is now manual-only, so VST-only
PRs do not rebuild the add-on module.
Version 0.4.75 adds per-hit WAV slice patterns: `start` and `end` accept
numbers, finite P/list patterns, or `var()`/`linvar()` values. Each triggered
hit must resolve to `0 <= start < end <= 1`; `loop=` remains a fixed Boolean.
The master output gain ceiling is now 250%, with its default unchanged at 100%.
Version 0.4.74 added the smoothed, state-persisted gain slider and bounded
WAV slicing/looping. Existing sample playback remains one-shot by default.
Version 0.4.73 fixes the Airwindows
persistent SynthDef startup rejection by
using their tested fixed control-index ABI instead of relying on runtime name
lookup. Version 0.4.72 added per-player stereo TapeDelay2 and Echojam inserts.
Note voices are summed before the insert, so delay state carries across
notes/chords and is not allocated per voice. Install the matching Airwindows
UGen pack; its SynthDefs use stereo `inbus` and `out` controls. The VST does not
rebuild or bundle the separate Airwindows module in its installer. Version 0.4.71 adds runtime loading of one precompiled SuperCollider `.scsyndef`
per file from `Portable/SynthDefs`, with filename/name checks and named control
mapping for `out`, `freq`, `amp`, `pan`, `gate`, `atk`, `rel`, and `sus`/
`sustain`. The editor exposes `userSynth("Name", notes, ...)`; it runs inside
the embedded engine with no `sclang`, `scsynth` process, or OSC server. A loaded
UGen module from `Portable/SCPlugins` can be used by a compatible SynthDef.
See `Portable/SynthDefs/README.txt`, `CustomPluck.scd`, and Example 94.
Version 0.4.68 adds `Clock.swing(amount=0.1)` with FoxDot's alternating
half-beat nudge, translated to sample-accurate onset offsets instead of a
separate server clock. Zero disables swing; the offset is safety-bounded to
less than half a beat. Its phase starts when a player is run or updated and
does not follow DAW transport. See Portable Example 93.
Version 0.4.67 extends `PZip(...)` and `PZip2(...)` to finite sample-symbol
patterns, matching FoxDot's cyclic least-common-multiple lacing. Zipped symbols
flatten into simultaneous sample hits; space-rests silence only their own lane.
Inputs and laced output are limited to 256 items, with at most eight symbols
per hit. Numeric PZip behavior is unchanged; see Example 92.
The Portable folder includes `OPTIONAL-SAMPLE-SETUP.html` with the exact WAV
folder layout and license-aware notes for separately downloaded sounds.
Version 0.4.66 added FoxDot-style `P & P` chord-zipping for finite
sample-symbol patterns. Inputs cycle to their least-common-multiple length
(maximum 256 expanded slots); each simultaneous group can contain up to eight
sample symbols, and spaces silence only their own layer. See Example 91.
Version 0.4.65 adds FoxDot-style grouped `sus=` values via `PGroup(...)`.
Sustain groups lace with chord tones and rhythm offsets, and expanded events
remain limited to 256 with eight simultaneous voices per player; see Example 90.
Version 0.4.64 added bounded nested `PRhythm(...)` groups, limited to eight
nesting levels; see Example 89. Version 0.4.63 extends
`PAlt`, `PStretch`, `PStutter`, `PShuf`, and `PJoin`
from sample-symbol inputs to bounded numeric note patterns, including rests
and chord groups. `PStutter` uses FoxDot's least-common-multiple cycling for
source/count patterns; Example 88 exercises all five transforms.
Version 0.4.62 supports FoxDot-style `sus > dur` overlap and repeated pitches.
Each player remains limited to eight simultaneous note voices, and sustain is
capped at eight pattern cycles for bounded scheduling; Example 87 demonstrates
overlapping repeated notes and a chord tail crossing the pattern loop.
Version 0.4.61 adds bounded numeric-note `PZip(...)` and filtered `PZip2(...)`
chord patterns; Example 86 demonstrates both. Version 0.4.60 added
`PQuicken(...)` delay clusters and a bounded `PRhythm(...)`
subset matching FoxDot's documented flat-list and Euclidean-tuple behavior;
Example 85 demonstrates both. Tuple groups in `dur=` create delayed repeats.
Nested duration patterns create bounded groups; grouped `sus=` values are now
supported through `PGroup(...)` and count toward the same event/polyphony caps.
Version 0.4.59 adds bounded `PDelay(...)` beat offsets and FoxDot's `PStrum(...)`
rhythm pattern; Example 84 combines them. Version 0.4.58 matches FoxDot's
slice-based `start` rotations for `PBeat(...)` and `PDur(...)`, including
negative and over-length offsets; see Example 83.
Version 0.4.57 added bounded FoxDot-compatible `PBeat(...)` and `PDur(...)`
duration patterns. Version 0.4.56 added sample-symbol `PShuf(...)` and `PJoin(...)`: shuffle a
complete phrase, or join small motifs before shuffling; see Examples 81–82.
Version 0.4.55 added
sample-symbol `PStretch(...)` and `PStutter(...)`, with
FoxDot-style repeat/cycle behavior and a 256-item cap. Numeric-note variants
were added in 0.4.63. Example 80 combines the original sample-symbol forms.
Version 0.4.54 added `PAlt(...)` for sample-symbol patterns. It cycles each
input to the least-common-multiple length, then alternates their items, matching
FoxDot's function behavior; inputs and expanded output are capped at 256 items.
Version 0.4.53 added
FoxDot-compatible `|` concatenation for finite sample-symbol
`P[...]` patterns: items from the left pattern are followed by items from the
right, and the result can still use `.reverse()` or `.rotate(n)`. Values must be
one sample symbol or a space-rest; `sample=` still selects the WAV variant.
FoxDot's `&` operator zips patterns into chord groups; finite sample-symbol
patterns now use that operator to trigger simultaneous samples. Version 0.4.52 added
sample-symbol `P[...]` sequences with `.reverse()` and `.rotate(n)`. Version
0.4.51 added per-hit selectors such as `|x1|`; 0.4.50 added nested play-string
groups: `[]` subdivides a step, `()` laces one item across successive pattern
cycles, `{}` generates a bounded eight-cycle variation, and adjacent `<>`
groups layer matching beats. The documented behavior was checked against
FoxDot's [sample-play demo](https://github.com/Qirky/FoxDot/blob/master/FoxDot/demo/03_playing_samples.py)
and [parser/player documentation](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/Patterns/Parse.py);
the embedded parser is an independent implementation. Selectors currently
accept one sample symbol and a single digit (0–9), matching FoxDot's
single-character parser behavior. Numeric P arithmetic over sample-symbol
patterns remains unsupported and is reported by the compatibility checker.
`Portable/OPTIONAL-SAMPLE-SETUP.html` explains how to keep a separately
licensed WAV pack outside the plugin download and install it under
`Portable/Samples`.

Use `p1 >> play("x-o-", dur=0.5)` with FoxDot's symbol-folder mapping under
`Portable/Samples`. A hyphen selects a closed-hat sample; whitespace is a
rest. `sample=` and signed `rate=` accept numbers or P/list patterns, plus
var()/linvar() controls. Sample selection stays discrete (linvar switches to
the nearest WAV at each segment midpoint); playback-rate linvar glides between
same-sign rates, while var() can switch direction. Rate=1 is natural speed;
negative rates reverse the native PlayBuf start and direction. A voice's rate
is evaluated at its trigger; it does not pitch-glide mid-sample. Unspecified
sus allows natural one-shot length, and explicit sus crops it. `start=0.0` and
`end=1.0` select the full file; smaller fractions slice within it. `loop=True`
loops only that slice and is bounded by `sus=` or `dur=`. All new controls are
optional; `start` and `end` also accept finite P/list patterns and
var()/linvar() for per-hit changes. Each hit must resolve to
`0 <= start < end <= 1`; leaving them out preserves the earlier one-shot
behavior. The editor's Master slider scales the final stereo output from 0%
(silence) to 250%, with 100% as unity, and smooths changes over 10 ms; older
saved states restore at their saved gain or unity when no value is present.
Gains above unity can clip downstream inputs.
Amp/pan, player stop/mute, basic effects and MIDI note output remain available.

The standalone C++ reader accepts PCM 8/16/24/32 and IEEE float32 mono/stereo
WAV (including matching extensible GUIDs), at 8–192 kHz. Stereo is currently
downmixed to one mono voice, then panned. Decoding, directory scanning and
allocation stay on the script worker, outside the audio/world lock. Immutable
buffers are published under that lock; both SC buffer mirrors are detached
before world cleanup so SC cannot free plugin-owned vectors. Sample-rate
teardown invalidates the cache generation, preventing stale buffer IDs from
being published. Errors leave active players unchanged.

File limit is 64 MiB, decoded cache budget 128 MiB, rate magnitude 0.0625–8;
the eight-note per-player polyphony limit still applies. Buffers are retained until
engine reset/unload; reopen after editing an already cached WAV. Further
P-pattern composition over sample strings, streaming, time stretching, non-WAV
codecs and third-party sample-library redistribution remain deferred.
The six tiny diagnostic WAVs are generated locally and offered under CC0.
Examples 70–73 exercise beat symbols, selection, reverse playback and
time-var sample/rate controls; example 76 tests per-hit sample indices,
example 77 tests finite P sample sequences, reversal and rotation, and example
78 demonstrates pattern concatenation followed by rotation, 79 tests PAlt
alternation, 80 combines PStretch/PStutter, 81 tests PShuf, 82 combines
PJoin/PShuf, 83 tests PBeat/PDur durations, 84 combines PDelay/PStrum, 85
demonstrates PQuicken/PRhythm, 86 combines PZip/PZip2, and 87 exercises
overlapping sustains. Example 88 uses all five transforms on numeric notes;
Example 89 demonstrates nested PRhythm timing groups.
Example 90 demonstrates grouped sustain lacing across chord tones and sample hits.
Example 100 demonstrates one-shot slicing and a bounded loop over a selected WAV
region. The UI master gain is a plugin-state control, not a DAW-automatable VST
parameter in this milestone.
`tools/test-portable-samples.py` compiles the
actual decoder and worker/cache code with undefined-behavior checks, then fills
an SC buffer from decoded floats and checks five native PlayBuf notes, direction/rate,
natural stopping, repeated startup and finite output. It does not use a disk
UGen or soundfile reader; Windows host validation is still required.

Version 0.4.47 fixes fresh-note startup in the new synth bank: pausing a graph
before its first SC render changes Graph_FirstCalc to Graph_NullFirstCalc;
Node_SetRun(1) does not reverse that state until a paused render has occurred.
Creating, pausing and immediately resuming every fresh note therefore left it
silent. Fresh note graphs now stay running and receive their controls before
the first World_Run. New-bank paused templates are no longer preallocated.
The private-bus/numeric-control audio regression reproduces the old silence
with --paused-start and passes all 26 graphs with the corrected lifecycle.
Example 69 adds an arranged pattern-composition/star/karp demo with a quiet sine
reference, rather than repeating the isolated four-note test.

It also fixes startup reporting and recovery: Run retries an unavailable
engine once on the script worker after the host has configured audio, rather
than returning the same generic "not ready" message indefinitely. Before host
audio setup it gives explicit host-processing instructions and does not guess
a sample rate. Startup failure stage/details are shown in the copyable editor
status, and successful startup clears stale errors. Teardown, sample-rate
changes and initialization share the same lock. The readiness regression test
uses the actual entry logic with a fake backend; it is not an audio/DAW test.
The lifecycle fix passes offline audio tests; confirmation in the user's
Windows host is still needed.

Version 0.4.46 adds all 26 remaining core-UGen FoxDot musical patches in one
batch: viola, nylon, arpy, dab, sawbass, dbass, prophet, pads, pasha, ambi,
sinepad, keys, klank, feel, glass, soft, quin, space, scratch, creep, snick,
swell, razz, sitar, star, and karp. Portable examples 43–68 test each one
individually. The Python checker, editor highlighting/completion, runtime
selector, and C++ definition registry cover the entire batch. The synth named
`swell(...)` is separate from the existing `swell=` effect control.

The checked-in bank is compiled by sclang from vendored, pinned original
FoxDot `.scd` files, not hand-built oscillator approximations. See
`foxdot-synths/README.md` for the trigger/routing and safety adaptations.
Windows builds still use cached SC outputs and do not compile UGens.
MdaPiano/Decimator, samples, and audio input are not added.

This Windows-first prototype embeds `libscsynth` and CPython 3.12 in a VST3 instrument. Its native editor evaluates a small Python pattern API, then passes bounded note events directly to the VST3 audio callback. The plugin renders up to eight independently running players, with up to eight notes per player (64 total), and emits matching MIDI notes on a separate channel per player. It does not launch `scsynth`, open an audio device, or use OSC.

## What it proves

- Sine, triangle, saw, and pulse/square voices are SynthDefs rendered by the embedded SuperCollider UGens; no server process is launched.
- Version 0.4.43 embeds twenty-seven FoxDot-derived SynthDefs—`bass`, `varsaw`, `dirt`, `jbass`, `noise`, `dub`, `growl`, `soprano`, `crunch`, `lazer`, `rave`, `bell`, `gong`, `marimba`, `pluck`, `twang`, `donk`, `squish`, `charm`, `spark`, `blip`, `scatter`, `orient`, `zap`, `fuzz`, `bug`, and `ripple`—based on [FoxDot’s `_SynthDefs.py`](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/SCLang/_SynthDefs.py), licensed CC BY-SA 4.0. Their graphs use bundled SuperCollider UGens; no extra runtime libraries are introduced. Bass and varsaw use FoxDot's source-style envelopes and amp scaling when the default `atk`/`rel` values are left untouched; charm, spark, and blip use their source-style amplitude motion. Examples 31–42 exercise the synth bank. The synth-to-graph index map now covers the whole bank, including `scatter` through `ripple`.
- Startup registers the synth definitions but no longer constructs every possible synth graph for every voice. A player’s selected synth graphs are created on the script worker as needed, and each voice keeps a bounded cache of two graph types. The audio callback never blocks on graph preparation; callbacks overlapping a non-realtime graph update output silence rather than waiting. Voice slots are partitioned by player, with eight-note polyphony plus room for overlapping release tails.
- The 0.4.39 Charm graph correction pairs each sine oscillator with its matching VarSaw and filter path. The 0.4.40 first self-contained batch adds scatter, orient, zap, fuzz, bug, and ripple using standard SuperCollider UGens.
- The VST continues to manage MIDI note-on/off independently of these audio envelopes. The reverb send remains alive for a short silent tail after an audible voice ends, so its control-rate fade completes before its SuperCollider graph stops.
- The host owns the audio callback; each VST block is split into SuperCollider's fixed 64-sample blocks and buffered for variable host block sizes.
- The plugin bundles supported official SuperCollider UGen modules in `Contents/Resources/SCPlugins` and scans `Portable/SCPlugins` for additional compatible Windows `.scx` modules. DiskIO_UGens and shared libsndfile are a **separate optional pack**, not part of the normal cached engine build. UIUGens is included for desktop mouse input; KeyState's Windows keyboard polling is disabled upstream. Six CC0 WAVs exercise the independent in-memory PlayBuf sample player, which does not use libsndfile. Restart the DAW after installing modules; avoid duplicate module copies. Ableton Link and Bela-specific modules remain excluded. SC modules use GPL terms; Boost headers have the included Boost Software License notice.
- User SynthDefs load from `Portable/SynthDefs` at engine startup, after compatible UGen modules. Each `.scsyndef` must contain one definition whose name matches its Python-safe filename, and it must expose an `out` control that writes one mono signal. `freq`, `gate`, `atk`, `rel`, and `sus`/`sustain` are mapped when present; `atk`/`rel` and `sus`/`sustain` receive seconds, while the plugin applies player amplitude, pan, filters, voice safety fades, and node cleanup. The folder is bounded to 256 definitions (8 MB per file/64 MB total). Example 94 uses a patch compiled offline from `Portable/SynthDefs/CustomPluck.scd`. The VST does not embed or launch `sclang`, and this first pass does not provide a GUI for authoring SynthDefs.
- The required CPython embeddable runtime files are copied into `Contents/Resources/Python`, with the Python license notice.
- The editor supports `tempo(bpm)`, `scale(name="major", root=60)`, `play(...)`, and eight independent players (`p1`-`p8`). It accepts direct FoxDot-style `Clock.bpm`, `Scale.default.set(...)`, `p1 >> saw(...)`, and `p1 >> play(...)` notation, with nested play-string subdivisions, lacing, bounded random variations, simultaneous layers, and per-hit selectors such as `|x1|`. The source text stays unchanged; embedded Python maps it to bounded VST events with no OSC or external server. Pattern helpers include `PRand`, `PWhite`, `reverse`, `rotate`, `repeat`, `palindrome`, `stutter`, `interleave`, `transpose`, and Euclidean rhythm generation with `euclid(hits, steps, note=60, rotation=0)`. `PRand` and `PWhite` accept an optional seed and a finite length from 1–256 (default 256); see Example 102. Sample players can select a persistent Airwindows delay separately from the sample source with `effect="AirwindowsTapeDelay2"` or `effect="AirwindowsEchojam"`; install the matching SynthDef and UGen first. Example 103 routes the sliced p1 rhythm through TapeDelay2 and leaves p2 dry. Finite `P[...]` literals convert to note lists with rests and chords; `+` and `-` support numeric transposition and FoxDot-style cyclic pattern lacing. Finite P patterns also support `.reverse()`, `.rotate()`, `.stutter()`, `.palindrome()`, plus whole-pattern `.repeat()`. For samples, finite `P[...]` literals accept one-character symbols and space-rests; `.reverse()`, `.rotate(n)`, FoxDot's left-to-right `|` concatenation, `P & P`, `PZip(...)`, and `PZip2(...)` work with sample-symbol patterns, followed by `play(...)`. Numeric P arithmetic over sample-symbol patterns remains unsupported. Example 92 covers sample-symbol PZip/PZip2, Example 91 covers `&` chord-zipping, and Examples 74–78 cover nested, indexed, transformed, and concatenated sample notation; see Portable examples 70–73 for the WAV player and time-varying controls. Other milestones and controls are documented below.
- The scale bank now includes 44 equal-tempered FoxDot scale names plus custom ascending semitone scales. `Scale.default = ...`, `Scale.default.set(...)`, `Root.default.set(...)`, and per-player `root=` support key and scale selection; Example 26 covers scale/chord groups, Example 27 tests global and per-player roots, Example 28 demonstrates FoxDot's Roman-numeral triads/seventh chords and `PGroup(...)`, and Example 29 demonstrates a gentler two-layer per-note chord strum, and Example 30 demonstrates player mute/unmute while patterns keep running. Just-intonation/frequency scales remain unsupported.
- The Windows editor has visible Run, Stop, Open, Save, and Save As controls for UTF-8 Python scripts. Run performs a static compatibility check first; known unsupported FoxDot calls, player aliases, controls, Clock/Scale/Root attributes, imports, malformed sample strings, and deferred sample syntax are reported with line numbers. Playback does not start until reported issues are addressed. The static check does not execute code and cannot guarantee dynamically computed values. The Copy Error button copies complete runtime exceptions and compatibility reports. FoxDot-style overlapping sustains, grouped `sus=` values, and repeated pitches are accepted under the eight-note per-player safety limit; exceeding it identifies the player and beat. The starter patterns are separate `.py` files in `Portable`; Open/Save As start there and Save writes to the current script path. The optional sample-folder instructions are in `Portable/OPTIONAL-SAMPLE-SETUP.html`. The editor prompts before replacing unsaved edits, and shows version and filename.
- VST3 project state stores the editor code. Loading a project restores the script in the editor but does not execute user code automatically.
- Python evaluation is isolated from the real-time callback; the callback reads a fixed-size pattern snapshot and generates audio/MIDI timing from the pattern's own tempo.
- A MIDI output event bus is exposed to the host.

## Prototype limits

- Version 0.4.45 rounds both SuperCollider hash-table capacities up to powers of two. The previous graph-definition capacity of 31 violated the pinned hash implementation's bit-mask probing requirement. Compile-time guards and a bounded probing regression test cover this constraint. Lifecycle timing traces remain enabled outside the audio callback; Windows CI preserves validator logs and treats timeout or failure as a workflow failure. Resolution of the host freeze still requires runtime verification.

- SuperCollider is pinned at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`; the VST3 SDK checkout is 3.8.1. The Windows script bundles CPython 3.12.10. GitHub Actions builds and validates the Windows VST3; audio, editor, and MIDI behavior still require testing in a real Windows DAW.
  - The engine uses `WorldOptions::mRealTime = false` so SuperCollider does not initialize its own audio driver. One hundred thirty-six fixed voice slots provide 17 slots per player for active notes and overlapping release tails. All four voice modes (sine, triangle, saw, and pulse/square) use the embedded SuperCollider SynthDef and bundled UGen modules. This is not a real-time-safety guarantee for every UGen or third-party module; only load trusted, compatible modules and test them in a DAW. Audit allocations, locks, and thread behavior before treating it as a production instrument.
- Audio pitch follows scheduled MIDI notes in 64-sample SuperCollider blocks, so audio onset is quantized to that block size; MIDI events retain sample offsets. Audio uses raised-cosine 10 ms attacks and up to 40 ms releases for natural note ends and explicit stops, plus smoothed voice-count mix adjustment. The envelope duration is unchanged; only the fade curve is smoother.
- Each player supports up to eight simultaneous notes, and up to eight players can run independently. MIDI is emitted on a separate channel per player. The four starter oscillator modes and twenty-seven embedded FoxDot-derived patches are available. Each player can set amplitude, stereo pan, low-pass cutoff, echo, and reverb. FoxDot-style `strum` staggers chord tones; `delay` offsets note starts in beats. Plugin-only `delaytime`, `echomix`, and `feedback` remain distinct extensions. Room/mix uses FreeVerb; the selector also offers FreeVerb2 and GVerb. Portable WAV playback uses the bundled PlayBuf, BufRd, and Phasor UGens for one-shots, normalized start/end slices, and loops bounded by `sus=` or `dur=`. It still does not stream from disk, pitch-preserve time stretch, record, or preserve stereo sample width. The DiskIO UGens and libsndfile are not used by this player. Player aliases beyond eight remain deferred. Unsupported calls and sample-string syntax produce explicit diagnostics instead of silent fallbacks. Arbitrary SynthDef evaluation and DAW transport following are not supported; the editor is Windows-only.
- User code is intentionally constrained but not a security sandbox. Pattern evaluation has a two-second Python loop guard; avoid untrusted scripts.
- SuperCollider is GPL-3.0, the Python runtime uses the PSF license, the VST3 SDK is MIT-licensed, and FoxDot-derived compatibility material is attributed and distributed under CC BY-SA 4.0. Package notices and license texts are included; binary distribution must comply with the relevant source and notice requirements. Review remaining transitive dependency terms before release.

## Build prerequisites

Use a SuperCollider source checkout at the pinned revision, a configured PIC-enabled SC build, the resulting `libscsynth` and UGen modules, the CPython 3.12.10 embeddable runtime, and a VST3 SDK checkout with its required submodules.

The normal Windows build uses `CMAKE_POSITION_INDEPENDENT_CODE=ON`, `LIBSCSYNTH=OFF`, `STATIC_PLUGINS=OFF`, and `NO_LIBSNDFILE=ON`; it does not bundle DiskIO or libsndfile. It builds/reuses the supported modules listed by the script, including UIUGens without X11. The engine archive must be PIC because it is linked into a shared VST3 module. The separate `UGens/DiskIO` pack builds only DiskIO_UGens and shared libsndfile 1.2.2, without rebuilding libscsynth or other modules; see its README and Portable Examples 106–108.

```sh
cmake -S spikes/vst3-callback -B build/vst3-callback -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVST3_SDK_ROOT=/path/to/vst3sdk \
  -DSC_SOURCE_DIR=/path/to/supercollider \
  -DSC_BUILD_DIR=/path/to/supercollider-build \
  -DSC_UGEN_PLUGIN_PATH=/path/to/supercollider-build/server/plugins \
  -DPYTHON_RUNTIME_DIR=/path/to/python-3.12-embed
cmake --build build/vst3-callback --target dexter_sc_tone
```

The build runs Steinberg's validator as a post-build check. `SC_UGEN_PLUGIN_PATH` is used only at build time to find platform-matched bundled modules. At runtime, the embedded SuperCollider loader searches `Contents/Resources/SCPlugins` and `Portable/SCPlugins`; the latter is the extension folder for user-added modules.

## Windows smoke build

The Windows script and GitHub Actions job use Visual Studio 2022 or 2026 x64, reuse the cached pinned SuperCollider engine/modules when available, download CPython's embeddable package, build the VST3 and run its required validator. The separate DiskIO workflow builds only DiskIO and shared libsndfile. Neither build requires the SuperCollider IDE, Qt, or sclang.

On Windows with Git, CMake 3.25 or newer, and the Visual Studio 2022 or 2026 C++ workload installed, run from the repository root:

```powershell
.\tools\build-windows-vst3.ps1
```

The GitHub Actions workflow also uploads the validated Windows VST3 bundle as a downloadable artifact when the job succeeds. This is a smoke-build path; Windows audio-host behavior still needs to be tested in a real VST3 host.
