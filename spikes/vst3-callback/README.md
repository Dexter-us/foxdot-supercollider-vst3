# Dexter U.S. FoxDot-inspired SuperCollider VST3 prototype

Version 0.4.186 makes the script worker state visible in the editor: Run becomes
a disabled Busy button while a request is processing, and duplicate submissions
are reported as in-progress instead of being presented as compatibility errors.
PShift is treated as a per-player note control; its slider appears only when a
selected player has `pshift` enabled.

Version 0.4.162 adds bounded `.controls` sidecars and live Players-panel
sliders for custom instrument SynthDefs. The bundled CustomPluck exposes
brightness and a sub-octave blend with zero defaults that preserve its prior
sine tone; CustomTone exposes waveform blend, filter cutoff, and resonance.
Ranges are explicit, validated against actual SynthDef parameters, and capped
at seven controls / 8 KiB per sidecar. Example 94 demonstrates CustomPluck;
Example 202 demonstrates CustomTone. Rebuild both demo SynthDefs, only when
their sources change, with `python tools/build-portable-user-synthdefs.py`.

Version 0.4.161 adds three SCLOrk-derived stereo insert effects: Phazer,
Decimator, and Crossover Distortion. They use SuperCollider core UGens only.
The loader detects each definition's output width, reserves isolated voice
buses, preserves stereo image, and rejects mixed-width or multichannel graphs.
These are player-chain effects, not instruments, so they appear in the Effects
sidebar category. The bundled portable SynthDefs are `SCLOrkPhazerFX`,
`SCLOrkDecimatorFX`, and `SCLOrkCrossoverDistortionFX`; source, attribution,
and the GPL-3.0 license accompany them. Example 201 demonstrates all three.
They do not require an SCPlugins module or a SuperCollider rebuild.

Version 0.4.159 adds a stereo-linked `LimutCompressor` insert with live FX
sliders, ranges, and `var()`/`linvar()` support. The built-in sine, saw, pulse,
and triangle calls are grouped under Waveforms. The optional LimutFX module also
contains a true two-input `LimutVocoder` UGen and a standalone bus example; it is
not yet exposed as a FoxDot insert because the embedded engine has no
per-player modulator/sidechain route. See `UGens/LimutFX/README.md`.

Version 0.4.158 adds three compiled LimutFX player-effect UGens: `LimutShifter`,
`LimutShimmer`, and `LimutGatedVerb`. They are optional members of
`LimutFX.scx`, with matching precompiled SynthDefs, effect-chain controls, and
Examples 197–198. The Shifter uses a two-head windowed delay; Shimmer feeds its
pitch-shifted signal into the pack's stereo reverb; GatedVerb shapes the reverb
tail with a smoothed input detector. See `UGens/LimutFX/README.md` for ranges,
installation, and test details. Building this optional module does not rebuild
SuperCollider itself.

Version 0.4.157 adds optional Limut-inspired Disperser and Fuzz inserts, and
fixes Players-panel chain selection so opening a chain selects its first insert.
LimutDuck now appears in its own Duck category. See Example 196.

Version 0.4.156 adds the optional Limut-inspired PingPong delay and Duck
inserts, with CC BY-SA 4.0 attribution, range-checked controls, and Example 195.
The separately compiled `LimutFX.scx` pack is not part of the VST3 build.

Version 0.4.155 adds bounded Pattern metadata helpers: `get_data()` returns the
live list-backed data view (unwrapping a single nested Pattern), `get_methods()`
lists the curated embedded API, `help()` returns help text without printing,
`new(data)` creates a bounded Pattern, `make()` flattens a single nested
Pattern in place, and `get_behaviour()` returns the default `None`. The
compatibility layer caps inputs and nesting at 256 nodes and 16 levels.
Example 194 demonstrates these helpers. Returning help text rather than
printing and making the supported method list curated are embedded-editor
adaptations.

Version 0.4.154 adds bounded `Pattern.true_copy(new_data=None)` and
`Pattern.convert_data(dtype=float)`. `true_copy` returns a shallow copy: the
top-level list is independent, while nested Patterns/groups are shared; an
optional finite `new_data` replaces the copied top-level values. Because this
engine stores Pattern values directly in a list, it cannot alias the root data
list as Renardo's internal Pattern object does. `convert_data` recursively
applies an approved pure one-argument converter, preserving nested patterns and
numeric chord groups when possible. Both methods enforce 16-level and
256-node/input/output bounds. Example 193 demonstrates copying and conversion.

Version 0.4.153 adds recursive Renardo-compatible `Pattern.transform(func)`.
It applies an approved pure one-argument mapper to numeric, text, or rest leaves,
recursing through nested patterns and chord groups while preserving their
structure. The callable and every result are validated, with the existing
16-level and 256-node input/output bounds. Example 192 demonstrates nested
transformation.

Version 0.4.152 adds bounded FoxDot-style elementwise `==`, `!=`, `<`, `<=`,
`>`, `>=`, and unary `~` for finite patterns. Comparisons produce bounded 0/1
patterns, preserve rests, recursively compare chord groups, and use finite
LCM-based lacing capped at 256 values. Unary `~` mirrors the pattern and nested
groups. Example 191 uses comparison masks to select notes.

Version 0.4.151 adds bounded Renardo `PZ12` and `PFibMod` generators.
`PZ12(tokens, p, length=256)` follows Renardo's documented experimental
two-token deficit ordering; `PFibMod(length=31)` provides the Fibonacci prefix,
limited so its values remain within the embedded note range. Example 190
combines weighted note selection, a Fibonacci-derived melody, and a steady
sample groove.
Renardo's callback-based `PTree`
generator is intentionally deferred because arbitrary user callbacks do not
fit the embedded engine's bounded evaluation model.

Version 0.4.150 adds a collapsible per-player effect-chain list. Selecting an
insert displays only that effect's supported controls, current values, and
numeric min/max ranges with units; dragging a slider changes the sound live,
and releasing it writes the value to the matching effect entry in the script.
The compatibility checker now validates `fx_*` names against each effect,
including inserts inside `effects=[...]`. It also supports Renardo's
`Clock.clear()` as a stop-all command for the eight local players and
`Seed.default` for repeatable random-pattern generation (with explicit
per-generator seeds taking priority). See Portable Examples 187-189.

Version 0.4.148 fixes sidebar insertion for Airwindows delays: Echojam and
TapeDelay2 are generated without the unsupported generic `fx_mix` control,
and the live FX Mix slider is hidden for those effects. Their required
compiled SynthDefs still need to be installed under `Portable/SynthDefs`.

Version 0.4.147 adds bounded FoxDot-compatible `Pattern.flatten()`. It
flattens one finite Pattern level into a simultaneous chord (`PGroup`), with a
maximum of eight numeric notes. Example 185 demonstrates nested chord flattening.
The compatibility audit also confirmed that `eq()`, `getslice()`, `i_rotate()`,
`i_reverse()`, `i_sort()`, `i_shuf()`, and `setitem()` are implemented and
allowlisted; a checker still rejecting those methods is likely from an older
plugin build.

Version 0.4.146 keeps the full player-control row visible and disabled until a
player is selected, so PShift remains discoverable. Clicking a `pN >>`
assignment in the editor selects that player in the Players panel. Example 184
demonstrates PShift and bitcrusher controls together.

Version 0.4.145 adds bounded `PIndex`, seeded `PxRand` without adjacent
repeats, and seeded weighted `PwRand`; see Portable Example 183. Synth and
Effect family categories now start collapsed, with a click to expand.

Version 0.4.144 adds the finite FoxDot generators `P10`, `PBern`, `PSine`,
`PTri`, and `PSum`, fixes `.expand()` compatibility checking for `Pvar`,
exposes PShift as a live semitone slider on any selected player, and makes
Synths/Effects category headings collapsible. Examples 181–182 cover the new
controls and generators.

Version 0.4.143 groups the Synths and Effects catalogs by family and adds live
Bits and Crush sliders for players using the native bitcrusher. Select such a
player to expose the controls; edits are smoothed in real time and written back
to the player call when released. See Example 180.

Version 0.4.142 alphabetizes the instrument catalog, expands the Effects list
with the embedded native controls and optional loaded inserts, and supports
right-click or drag-and-drop from either sidebar onto a player line. A bare
`p3 >>` gets a starter voice; choosing an effect there first creates a default
sine player. Right-click sidebar rows for About details and effect ranges in
`Portable/179-editor-catalog-about.html`. Example 179 demonstrates the workflow.
Catalog actions edit source; click Run to apply. No SuperCollider server or OSC
connection is introduced.

Version 0.4.141 added live per-player Level, Pan, Cutoff, and FX-mix sliders;
audio-thread updates are queued and smoothed without rerunning the script.
Releasing a slider writes its scalar value into the selected player's source
line. FX Mix controls the first insert in a chain. See Portable Example 178.

Version 0.4.140 adds bounded FoxDot-style `pshift` (semitones, -24 to +24)
and native per-player `bits`/`crush` controls. `bits=0` bypasses bit-depth
reduction; 1–24 quantizes the player's summed signal, while `crush=1–128`
reduces the hold rate. Both bitcrusher controls are smoothed and support scalar,
`var()`, and `linvar()` input. `pshift` is applied per note, accepts scalar,
finite `P[...]`, `var()`, and `linvar()` values, and transposes WAV playback as
well as pitched synths. The native effect needs no external UGen or
SuperCollider rebuild. Portable Example 177 demonstrates the controls.

Version 0.4.139 adds native FoxDot-style `coarse` pulse gating and a bounded
`formant` control for synth and WAV players. `coarse` ranges from 0–32;
`formant` ranges from -1 (bypass) to 7. Both accept scalar, finite Pattern,
`var()`, and `linvar()` values and appear in the live Effects panel. The
formant DSP is a stable Formlet-style resonator approximation; it does not load
an external SynthDef or rebuild SuperCollider. Portable Example 176 demonstrates
both controls.

Version 0.4.136 adds self-contained Phaser and Tremolo per-player inserts using
stock SuperCollider UGens only. Their controls are smoothed and accept scalar,
`var()`, and `linvar()` values. Portable Examples 171–172 demonstrate the
effects; no external UGen pack or SuperCollider rebuild is required.

Version 0.4.133 adds bounded FoxDot `chop` and `spin` effects to synth and WAV
players. `chop` gates the note into pulses across its sustain (0–32); `spin`
adds the original stereo sine-pan motion (0–40). Both accept scalar, finite
Pattern, `var()`, and `linvar()` controls and appear in the live Effects panel
and compatibility checker. Portable Example 165 demonstrates both.

Version 0.4.132 adds FoxDot-compatible `vibdepth`, `slide`, `slidefrom`, and
`bend` note controls, including `slidedelay` and `benddelay`. The slide and bend
controls follow FoxDot's bundled SynthDef amplitude envelopes; they do not
retune pitch. Scalar, `var()`/`linvar()`, and finite `P[...]` control values are
bounded and evaluated per note. `vib` is the rate; `vibdepth` is the depth.
`vibdepth` and the older `depth=` name share one control and cannot both be
specified; omitting both preserves the existing `depth` default. Supported
ranges are 0–10 for depth, -1–4 for slide/bend amounts, and 0–1 for delay
fractions. `vibdepth` applies to bundled synth graphs that expose FoxDot's
vibrato controls; the simple sine/saw/triangle/pulse oscillator aliases do not
yet have a vibrato input. Portable Example 164 uses `viola` for that control.

Version 0.4.131 adds smoothly modulated, native per-player FoxDot `shape` and
`drive` effects for synths and WAV sample players, plus Portable Example 163.
Both accept 0–4 (FoxDot's usual range is 0–1); the self-contained VST DSP
curves are FoxDot-style controls, not bit-for-bit SuperCollider effect
emulations.

Version 0.4.130 adds a live local-engine browser with Players, Synths, Effects,
and Samples tabs. Player rows track active/muted state, synth selection, and
per-player effects; selected players can be muted or stopped without editing
their other players. The Samples tab lists WAVs in the Portable letter banks and
opens that folder. The compatibility checker now has direct coverage for
`.eq()`, `.getslice()`, `.i_rotate()`, `.i_reverse()`, `.i_sort()`, `.i_shuf()`,
and `.setitem()` so supported finite Pattern internals are not reported as
unknown methods. Portable Example 162 demonstrates per-letter WAV banks,
`sample=` selection, and the per-hit `|x1|` override.

Version 0.4.129 adds bounded `Pattern.eq()`/`.ne()` comparison masks with
right-side cycling; in-place `.i_rotate()`, `.i_reverse()`, `.i_sort()`, and
`.i_shuf()`; integer `.setitem()` mutation; and explicit cyclic `.getslice()`.
Mutators preserve Pattern identity so aliases see edits, while `.copy()` stays
independent. Sorting is atomic on incomparable values, and all source/result
sizes are bounded to 256 with 16 nested levels. Example 161 demonstrates masks,
selection, mutation, and slices.

Version 0.4.128 adds bounded FoxDot-style `Pattern.getitem()`: integer indices
wrap cyclically, nested finite Patterns advance by cycle, and finite index
Patterns/tuples return bounded Pattern results. Python `pattern[index]` remains
ordinary non-wrapping list indexing. Inputs/results are capped at 256 items
with 16 nested levels. Portable Example 160 demonstrates cyclic selection.

Version 0.4.127 adds bounded `Pattern.items()`, returning a materialized list
of `(index, value)` pairs. It snapshots nested values, preserving the source
Pattern from edits through the returned pairs, and is capped by the existing
256-value and 16-level copy limits. Portable Example 159 uses the indexed
values to build a reordered phrase.

Version 0.4.126 adds bounded FoxDot-style `Pattern.string()`. It joins finite
sample symbols, formats nested patterns with parentheses and chords compactly,
renders rests as spaces for the embedded play-string parser, and caps output at
256 values, 16 nesting levels, and 4096 characters. Portable Example 158 turns
Patterns into sample-player rhythms.

Version 0.4.125 adds FoxDot-style Pattern `.set(index, value)` and
`.alt(other)`. `set` mutates the selected item and returns the same Pattern;
`alt` returns a fresh Pattern without changing its source. Both validate finite
numeric/sample data and respect the 256-item bound. Portable Example 157
demonstrates shared updates and a non-mutating alternate phrase.

Version 0.4.124 adds bounded in-place Pattern `.insert(index, item)`,
`.pop(index=-1)`, and `.remove(item)`. Inserted values are validated and copied,
shared references see mutations, and `.copy()` remains independent. Indexes use
Python list clamping/error behavior; the 256-item cap still applies. Portable
Example 156 demonstrates insert, pop, remove, and a pre-mutation copy.

Version 0.4.123 adds in-place Pattern `.append(item)` and `.extend(sequence)`,
matching FoxDot's `None` return behavior. Inputs are copied before insertion,
shared references to the destination Pattern see the mutation, and numeric-note
patterns cannot be mixed with sample-symbol patterns. Only bounded finite
sequences are accepted; top-level output, nested values, and groups are limited
to 256 items, 256 values, and 8 notes respectively. Portable Example 154 shows
shared mutation and an independent `.copy()`.

Version 0.4.122 adds bounded Pattern `.str()` conversion. Numeric values become
text, while sample symbols and rests remain unchanged; numeric text carries an
internal marker so it is not treated as sample notation or a numeric chord.
Nested groups are preserved, with a 256-value and 16-level limit. Portable
Example 153 demonstrates the separate sample-string and numeric-note paths.

Version 0.4.121 adds bounded Pattern `.int()` and `.float()` conversions.
They recursively convert finite numeric notes and chord members, preserve rests
and nested pattern/group structure, and leave the source unchanged. Inputs are
limited to 256 values and 16 nesting levels; sample strings, booleans, and
non-finite values are rejected. Portable Example 152 demonstrates both.

Version 0.4.120 adds bounded Pattern `.asGroup()` and `.group()` aliases.
Each wraps a flat finite numeric note pattern as one simultaneous chord, matching
FoxDot; the embedded chord limit is 8 notes, and empty, nested, sample, rest,
and non-finite inputs are rejected. Portable Example 151 demonstrates both.

Version 0.4.119 adds bounded, non-mutating Pattern `.copy()`. It recursively
copies finite pattern/group containers, keeps chord-group types, and enforces
256 values and 16 nesting levels. Editing a copied nested pattern does not
change the source. Portable Example 150 demonstrates an independent phrase.

Version 0.4.118 adds FoxDot-style Pattern `.every(n, method, *args)`.
It repeats the original finite phrase `n-1` times and appends one transformed
copy. The embedded subset allows `reverse`, `rotate`, `mirror`, `shuffle`, and
`stutter`, caps output at 256 items, and rejects arbitrary method names and
keyword arguments. Portable Example 149 demonstrates the transforms.

Version 0.4.117 adds `.limit(func, value)` for finite numeric note patterns.
Only the pure built-in `sum`, `max`, and `len` aggregators are accepted; the
pattern cycles until the threshold is reached, with a 256-item output cap.
Unreachable `max` thresholds, empty patterns, nested chords, samples, rests,
and non-finite inputs are rejected rather than allowing an unbounded loop.
Portable Example 148 demonstrates the three supported aggregators.

Version 0.4.116 adds bounded, non-mutating `.concat(other)`, `.add(other)`,
`.abs()`, `.all(predicate)`, and `.startswith(value)` pattern helpers.
Concatenation preserves groups and rejects mixed numeric/sample patterns;
predicates must be pure one-argument functions. Output is capped at 256 items
with numeric groups bounded to 8 notes and nesting to 16 levels. Portable
Example 147 demonstrates the new helpers.

Version 0.4.115 adds bounded FoxDot-style `.zipx(other)` for numeric/chord and
sample-symbol patterns. It cycles both inputs to their LCM length and preserves
each zipped pair's nested structure; sample pairs play as simultaneous layers.
Output is capped at 256 items, nesting at 16 levels, and each hit at 8 notes or
symbols. Portable Example 146 demonstrates both forms.

Version 0.4.114 adds bounded `.duplicate(n)` and `.iter(n)` aliases for
FoxDot-style finite phrase repetition. Both preserve nested chord/pattern
items, accept counts from 0 to 256, and cap expanded output at 256 items.
Portable Example 145 compares both methods on nested phrases.

Version 0.4.113 adds bounded callable `.map(func)` and FoxDot-compatible
`.submap(mapping)`. Callable mapping recursively transforms nested notes and
chords; it accepts pure single-argument arithmetic/conditional functions and
selected pure built-ins, rejecting attribute/global side effects. Dictionary
`.map()` remains available with its existing missing-key default of zero;
`.submap()` instead leaves unmatched values unchanged. Input/output are capped
at 256 values and nesting at 16 levels. Portable Example 144 demonstrates both.

Version 0.4.112 adds bounded FoxDot-style `.sample(n)` and `.deep_shuffle(n)`.
Both accept an optional 32-bit `seed` for repeatable results; sampling is
without replacement by source position, and deep shuffle emits independently
shuffled whole-pattern copies while preserving nested groups. Output is capped
at 256 items. Portable Example 143 demonstrates both.

Version 0.4.111 adds finite-pattern `.compress(selector)` and `.select(selector)`.
Both cycle truthy/falsy masks; `compress()` is bounded by its source length,
while `select()` uses the FoxDot LCM span and rejects expansions over 256.
All-zero masks produce an empty, silent pattern. Dynamic var/linvar masks are
not part of this finite subset. Portable Example 142 demonstrates note and
sample-symbol masks.

Version 0.4.110 adds bounded FoxDot-style `PSq`, `PRange`, `PStep`,
`PEuclid`, and `PEuclid2`; `.norm()` and `.undup()` aliases; and clearer
compatibility diagnostics for unsupported methods called directly on `P[...]`.
Generated patterns are limited to 256 items. Portable Example 141 demonstrates
numeric sequences and a Euclidean sample rhythm.

Version 0.4.109 lets `Pvar` use `PRhythm` grouped durations and extends numeric
`PZip`/`PZip2` to combine chord groups. Grouped offsets retain their bounded
sub-hit timing across phrase switches; zipped chords preserve rests and are
limited to eight notes per event. Portable Examples 139–140 cover both changes.

Version 0.4.108 adds bounded `Pvar` phrase switching for numeric note and
sample-symbol patterns. `Pvar([phrase_a, phrase_b], [8, 8])` repeats each
finite phrase for its beat span, then switches and restarts that phrase at its
first item. Durations may be one value shared by all phrases or one per phrase.
`dur` may be one fixed number, a finite numeric pattern, or bounded `var`/
`linvar`. For predictable looping, every phrase span must end exactly on a
note-duration boundary; finite duration patterns must finish on their starting
phase, while a time-var's full cycle must divide the Pvar cycle evenly.
Time-varying duration values are evaluated at each note start. Pvar remains limited to 8 phrase states,
256 expanded note slots per player, and a 4096-beat full phrase cycle. Examples
137–138 demonstrate fixed/patterned durations and var/linvar note lengths.

Version 0.4.107 adds `Clock.bpm = var(values, durations)` and
`Clock.bpm = linvar(values, durations)`, with a prepared, allocation-free tempo
map shared by audio/MIDI scheduling. Portable Example 136 has a clear stepped
tempo and a commented ramp alternative. Tempo durations use beats and do not
restart with each player's shorter pattern loop. Other time-varying controls
follow the mapped beat; their control position is interpolated within each
64-sample SC block. Beat-based echo follows current BPM; seconds-based delay
and Airwindows time parameters remain in seconds.

The supported tempo syntax and trigger-time `sus`/delay conversion follow
[FoxDot's TempoClock](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/TempoClock.py)
and [Player OSC message construction](https://github.com/Qirky/FoxDot/blob/master/FoxDot/lib/Players.py):
already-sounding notes keep the sustain duration chosen at their trigger
tempo, and WAVs are not time-stretched. This is still the plugin's restart-on-Run
clock: final global BPM applies to every player updated by that Run, starting
from the same phase. It does **not** implement desktop FoxDot's next-bar
assignment behavior or DAW transport sync. Unchanged players are not restarted
by a script which only changes a different player.

TimeVar limits remain 1–8 finite BPM values, each at least 0.001, with positive
segment durations up to 4096 beats. There is no added fixed upper BPM limit;
existing event/voice capacities still apply and acceleration can increase
overlap. Regression checks: `python3 tools/test-clock-tempo.py` (requires g++)
exercises the actual audio/MIDI scheduling code with UBSan, constant/stepped/
ramped clocks, loop phase, gate lengths, and 100,005 mapping round trips.
`tools/test-player-result-parser.py` round-trips the Portable examples through
the actual compiled result parser.

Version 0.4.106 adds bounded `PSquare(length=256)`, returning the deterministic
square-number sequence `0, 1, 4, 9, ...` as a finite pattern. Its output length
is limited to 1–256 values. Portable Example 135 demonstrates pitch mapping
with modulo while keeping the generated sequence finite.

Version 0.4.105 adds finite `PDelta(deltas, start=0, length=256)`. It starts
with `start`, then repeatedly adds the delta sequence; a scalar delta repeats.
The finite output is capped at 256 notes and accumulated pitch values are
bounded to ±1,000,000. Portable Example 134 demonstrates a lead and bass
contour.

Version 0.4.104 adds finite `PChain(mapping, length=256, seed=None)`. It
starts at the first mapping key, chooses from that state's destinations, and
holds a terminal value if it has no mapping. It supports numeric-note or
sample-symbol chains, at most 32 states and 256 transition choices. Portable
Example 133 demonstrates a melodic chain and a sample-symbol drum chain.

Version 0.4.103 adds finite, seeded `PWalk(max=7, step=1, start=0)`. It
follows FoxDot's bidirectional walk and turning thresholds, with optional
finite step patterns, `length=1..256`, and `seed=`. The walk turns when it
reaches or crosses ±max, matching FoxDot's step behavior. Portable Example 132
demonstrates a melodic and bass walk.

Version 0.4.102 adds bounded FoxDot-style `.true_shuffle(n)` and
`.shufflets(n)`. `true_shuffle()` shuffles the expanded finite phrase, keeping
chords together; `shufflets()` creates independently shuffled numeric chord
groups. Results use the seeded random stream and are limited to 256 items.
The chord-group subset is limited to eight numeric notes. Portable Example 131
demonstrates both.

Version 0.4.101 adds bounded FoxDot-style `.map(mapping)` and `.pivot(index)`.
`map()` applies a dictionary recursively to nested patterns and chords, with
unmapped values becoming zero as in FoxDot. `pivot()` mirrors and rotates a
phrase around one or more whole-number steps. Output is limited to 256 values
and nested transforms to 16 levels. Portable Example 130 demonstrates both.

Version 0.4.100 adds bounded FoxDot-style `.zip()`, `.deepzip()`, and
`.deeprzip()` helpers. `zip()` pairs cyclic numeric or sample-symbol patterns
into chord-like groups; the deep variants preserve nested phrase structure, and
`deeprzip()` reverses the paired values. Deep output is limited to 256 pairs and
16 nested levels. Portable Example 129 demonstrates all three.

Version 0.4.99 adds FoxDot-style `.normalise()` for finite numeric patterns and
`.choose()` for selecting one item from a finite numeric or sample-symbol
pattern. Normalisation maps a phrase to 0..1 and maps constant phrases to zero;
`choose()` uses the embedded runtime's seeded random stream at evaluation time.
Portable Example 128 demonstrates a normalized pitch-degree contour and one
selected pitch.

Version 0.4.98 adds FoxDot-style `.trim(size)` and `.ltrim(size)` for taking
the front or tail of a finite pattern. Both follow Python/FoxDot slicing,
including negative sizes; `ltrim(0)` returns the full pattern, matching
FoxDot's `self[-size:]` implementation. Portable Example 127 demonstrates
splitting a phrase into call and response.

Version 0.4.97 adds a bounded FoxDot-style `.layer("method", ...)` subset.
It zips a pattern with the result of an allowlisted transform, preserving
FoxDot's chord-grouping behavior; output is capped at 256 events. Arbitrary
Python callables are intentionally not accepted by the embedded API.
Portable Example 126 demonstrates reverse and rotate layers.

Version 0.4.96 adds FoxDot-style finite-pattern `.replace(old, new)`.
It recursively replaces matching note values in nested patterns/chords and
also works on sample-symbol patterns, preserving the pattern's structure.
Nested input is limited to 16 levels. Portable Example 125 demonstrates it.

Version 0.4.95 adds FoxDot-style finite-pattern `.sort()` and
`.unduplicate()`. `sort()` orders comparable pattern items ascending;
`unduplicate()` removes adjacent repeats without changing the order or removing
later, non-adjacent repeats. Portable Example 124 demonstrates both.

Version 0.4.94 adds FoxDot-style finite-pattern `.arp(intervals)` and
`.splice(pattern, ...)`. `arp()` repeats each numeric note by the interval
count and adds the interval phrase; `splice()` cyclically interleaves numeric
phrases or sample-symbol patterns. Results are bounded to 256 items.
Portable Example 123 demonstrates both.

Version 0.4.93 adds FoxDot's native finite-pattern `.invert()` method. Numeric
notes reflect around the highest note in their phrase; rests remain rests and
chord groups are inverted independently. The existing `transpose(pattern,
interval)` helper remains the transposition syntax—FoxDot does not define
`.transpose()` as a pattern method. Portable Example 122 demonstrates both.

Version 0.4.92 adds bounded FoxDot method forms `.stretch(size)`, `.loop(n)`,
and `.shuffle(n)` to finite `P[...]` patterns. `stretch` repeats values to a
target length; `loop` repeats the full phrase; `shuffle` emits independently
shuffled copies while keeping chord items together. Each operation is limited
to 256 expanded items. Portable Example 121 demonstrates all three.

Version 0.4.91 adds bounded `P` pattern methods `mirror()`, `accum()`, and
`swap(n)`. `mirror()` recursively reverses nested patterns; `accum()` creates
cumulative numeric contours; `swap(n)` reverses each block of `n` items.
Portable Example 120 demonstrates the three helpers.

Version 0.4.90 lets `Root.default.set()` take bounded `var()` and `linvar()`
values. The global root is sampled at note onsets, and fixed per-player roots
still override it. Changing `Scale.default` alone preserves the current root.
Portable Example 119 demonstrates stepped and semitone-gliding key changes.

Version 0.4.89 adds FoxDot-style per-player `root=` and shared `Group.root`.
Root changes are applied at note onsets; `var()` and `linvar()` values are
rounded to MIDI semitones, while note names and numeric pitch classes/MIDI
notes remain available for fixed roots. Portable Example 118 demonstrates
shared transposition and a changing root.

Version 0.4.88 adds bounded `var()`/`linvar()` motion to `Group.degree`.
Pitch is sampled and rounded to a scale degree at each note onset (not a
continuous glide); rests remain rests and existing chord intervals are
retained. Values are limited to -128..127 and must still resolve to MIDI
pitches 0..127. Portable Example 117 demonstrates the feature.

Version 0.4.87 extends FoxDot-style player groups with shared `degree`, `dur`,
`sus`, `oct`, and `scale` assignments. Pitch controls apply to numeric players;
timing controls also work on sample players. `oct` and `dur` accept `var()` and
`linvar()`. Portable Example 116 demonstrates the controls.

Version 0.4.86 adds FoxDot-style player groups: `Group(p1, p2)` broadcasts
supported sound/effect controls and `.stop()`, `.mute()`, `.unmute()`,
`.solo()`, `.only()`, and bounded scheduling to its members. `Master()` selects
currently active players, and `p_all` contains all eight slots. Group controls
can be assigned before or after player patterns, including `var()` and
`linvar()` controls. Portable Example 115 demonstrates the feature.

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
- User SynthDefs load from `Portable/SynthDefs` at engine startup, after compatible UGen modules. Each `.scsyndef` must contain one definition whose name matches its Python-safe filename, and expose an `out` control that writes either one mono or two stereo channels. Stereo SynthDefs keep their center image; the player's pan setting balances the left/right channels, while mono SynthDefs retain equal-power panning. `freq`, `gate`, `atk`, `rel`, and `sus`/`sustain` are mapped when present; `atk`/`rel` and `sus`/`sustain` receive seconds, while the plugin applies player amplitude, filters, voice safety fades, and node cleanup. The folder is bounded to 256 definitions (8 MB per file/64 MB total). Example 94 uses a patch compiled offline from `Portable/SynthDefs/CustomPluck.scd`. The VST does not embed or launch `sclang`, and this first pass does not provide a GUI for authoring SynthDefs.
- The required CPython embeddable runtime files are copied into `Contents/Resources/Python`, with the Python license notice.
- The bounded pattern helpers `P[...].mirror()`, `.accum(n)`, and `.swap(n)` match FoxDot's nested reversal, cumulative-sum, and reversed-block behavior; numeric `accum` lengths and expanded results are capped at 256. See Portable Example 120.
- Finite `P[...]` patterns also support FoxDot-style `.arp(intervals)` for numeric-note arpeggiation and `.splice(pattern, ...)` for cyclic interleaving of numeric or sample-symbol patterns; outputs are capped at 256 items. See Portable Example 123.
- Finite `P[...]` patterns also support FoxDot-style `.sort()` and `.unduplicate()`. Sorting follows Python's ascending ordering for comparable items; `unduplicate()` removes consecutive repeats only. See Portable Example 124.
- Finite `P[...]` patterns also support FoxDot-style `.replace(old, new)` for recursively replacing matching values in note, chord, and sample-symbol patterns. The pattern structure is preserved and nested depth is limited to 16. See Portable Example 125.
- Finite `P[...]` patterns also support a bounded `.layer("method", ...)` subset that zips the original phrase with its transformed copy. Only existing allowlisted pattern methods are accepted; arbitrary callables are not. Results are capped at 256 events. See Portable Example 126.
- Finite `P[...]` patterns also support FoxDot-compatible `.trim(size)` and `.ltrim(size)` slices for the front and tail of a phrase. Python slice behavior is preserved, including negative sizes and `ltrim(0)` returning the full pattern. See Portable Example 127.
- Finite `P[...]` patterns also support FoxDot-style `.normalise()` for numeric contours and `.choose()` for one seeded random selection from a numeric or sample-symbol pattern. `.normalise()` requires a non-empty numeric phrase; constant phrases become zero. `choose()` returns one value when the script evaluates. See Portable Example 128.
- Finite `P[...]` patterns also support `.zip()`, `.deepzip()`, and `.deeprzip()`. `zip()` makes paired chord-like groups; the deep forms keep nested phrase structure, with `deeprzip()` reversing the pairs. The bounded subset accepts only the built-in output type and limits deep results to 256 pairs/16 levels. See Portable Example 129.
- Finite `P[...]` patterns also support FoxDot callable `.map(func)` and `.submap(mapping)`, plus the legacy dictionary `.map(mapping)` form. Callable mapping recursively transforms nested values; dictionary map retains the plugin's prior zero default for missing keys, while submap preserves them. `pivot()` rotates and mirrors the phrase around selected integer steps. Results are bounded to 256 values and 16 nested levels. See Portable Examples 130 and 144.
- Finite `P[...]` patterns also support bounded `.true_shuffle(n)` and `.shufflets(n)`. `true_shuffle()` shuffles expanded nested notes while keeping chord groups intact; `shufflets()` creates seeded shuffled numeric chord groups. See Portable Example 131.
- `PWalk(max=7, step=1, start=0, length=256, seed=None)` creates a finite, seeded FoxDot-style bidirectional random walk. A finite numeric step pattern cycles; output length is limited to 256 items. See Portable Example 132.
- `PChain(mapping, length=256, seed=None)` creates a finite seeded transition chain from a dictionary. The first key is the start; values are possible next notes or sample symbols. Unmapped terminal values repeat, and the graph is limited to 32 states/256 choices. See Portable Example 133.
- `PDelta(deltas, start=0, length=256)` emits the start, then accumulates a cyclic finite numeric interval pattern; scalar deltas repeat. Output is limited to 256 items, with accumulated values bounded to ±1,000,000. See Portable Example 134.
- `PSquare(length=256)` returns the finite square-number sequence starting at zero. Output length is limited to 1–256. See Portable Example 135.
- `PRange(start, stop=None, step=1)` follows FoxDot's integer range behavior, including automatic step direction, with a maximum of 256 notes. `PSq(a=1, b=2, c=3)` returns bounded powers in the FoxDot form `a**b` through `(a+c-1)**b`; results must stay finite and within ±1,000,000. `PStep(n, value, default=0)` repeats an n-step pattern with `value` on its last step. `PEuclid(n, k)` returns a 1/0 pulse pattern, and `PEuclid2(n, k, lo, hi)` maps those hits/rests to user values; both are limited to 256 steps. Example 141 compares numeric patterns and a sample-symbol rhythm with space rests.
- Finite `P[...]` patterns support FoxDot-style `.compress(selector)` and `.select(selector)` masks for numeric and sample-symbol patterns. `compress()` cycles the mask but emits at most the source length; `select()` cycles to the least-common-multiple span and rejects expansions over 256. An all-zero mask yields an empty, silent player pattern; dynamic var/linvar selectors are not included. See Portable Example 142.
- Finite `P[...]` patterns support `.sample(n, seed=None)` without replacement by source position and `.deep_shuffle(n=1, seed=None)` for repeated shuffled copies that keep nested groups intact. A supplied integer seed in `0..4294967295` makes results reproducible; outputs are capped at 256 items. Example 143 demonstrates both.
- FoxDot aliases `.norm()` and `.undup()` are also available for `.normalise()` and `.unduplicate()`. The compatibility checker now diagnoses unknown methods chained directly from finite `P[...]` patterns before execution.
- Finite `P[...]` patterns also support `.invert()`, which reflects numeric notes around the phrase's highest note while preserving rests and independently inverting chord groups. The existing `transpose(pattern, interval)` helper transposes notes and chords; FoxDot has no pattern `.transpose()` method. See Portable Example 122.
- Finite `P[...]` patterns also support FoxDot method forms `.stretch(size)`, `.loop(n)`, and `.shuffle(n)`. They accept bounded scalar arguments; `stretch` and `loop` also lace finite size/count patterns, and all expanded output is capped at 256 items. Shuffling is seeded per script evaluation and preserves nested chord items. See Portable Example 121.
- The editor supports `tempo(bpm)`, `scale(name="major", root=60)`, `play(...)`, and eight independent players (`p1`-`p8`). It accepts direct FoxDot-style `Clock.bpm`, `Scale.default.set(...)`, `p1 >> saw(...)`, and `p1 >> play(...)` notation, with nested play-string subdivisions, lacing, bounded random variations, simultaneous layers, and per-hit selectors such as `|x1|`. The source text stays unchanged; embedded Python maps it to bounded VST events with no OSC or external server. Pattern helpers include `PRand`, `PWhite`, `reverse`, `rotate`, `repeat`, `palindrome`, `stutter`, `interleave`, `transpose`, and Euclidean rhythm generation with `euclid(hits, steps, note=60, rotation=0)`. `PRand` and `PWhite` accept an optional seed and a finite length from 1–256 (default 256); see Example 102. Sample players can select a persistent Airwindows delay separately from the sample source with `effect="AirwindowsTapeDelay2"` or `effect="AirwindowsEchojam"`; install the matching SynthDef and UGen first. Example 103 routes the sliced p1 rhythm through TapeDelay2 and leaves p2 dry. Finite `P[...]` literals convert to note lists with rests and chords; `+` and `-` support numeric transposition and FoxDot-style cyclic pattern lacing. Finite P patterns also support `.reverse()`, `.rotate()`, `.stutter()`, `.palindrome()`, plus whole-pattern `.repeat()`. For samples, finite `P[...]` literals accept one-character symbols and space-rests; `.reverse()`, `.rotate(n)`, FoxDot's left-to-right `|` concatenation, `P & P`, `PZip(...)`, and `PZip2(...)` work with sample-symbol patterns, followed by `play(...)`. Numeric P arithmetic over sample-symbol patterns remains unsupported. Example 92 covers sample-symbol PZip/PZip2, Example 91 covers `&` chord-zipping, and Examples 74–78 cover nested, indexed, transformed, and concatenated sample notation; see Portable examples 70–73 for the WAV player and time-varying controls. Other milestones and controls are documented below.
- The scale bank now includes 44 equal-tempered FoxDot scale names plus custom ascending semitone scales. `Scale.default = ...`, `Scale.default.set(...)`, `Root.default.set(...)`, and per-player `root=` support key and scale selection; Example 26 covers scale/chord groups, Example 27 tests global and per-player roots, Example 28 demonstrates FoxDot's Roman-numeral triads/seventh chords and `PGroup(...)`, Example 29 demonstrates a gentler two-layer per-note chord strum, Example 30 demonstrates player mute/unmute while patterns keep running, and Example 119 tests global root `var()`/`linvar()` controls. Just-intonation/frequency scales remain unsupported.
- The Windows editor has visible Run, Stop, Open, Save, and Save As controls for UTF-8 Python scripts. Run performs a static compatibility check first; known unsupported FoxDot calls, player aliases, controls, Clock/Scale/Root attributes, imports, malformed sample strings, and deferred sample syntax are reported with line numbers. Playback does not start until reported issues are addressed. The static check does not execute code and cannot guarantee dynamically computed values. The Copy Error button copies complete runtime exceptions and compatibility reports. FoxDot-style overlapping sustains, grouped `sus=` values, and repeated pitches are accepted under the eight-note per-player safety limit; exceeding it identifies the player and beat. The starter patterns are separate `.py` files in `Portable`; Open/Save As start there and Save writes to the current script path. The optional sample-folder instructions are in `Portable/OPTIONAL-SAMPLE-SETUP.html`. The editor prompts before replacing unsaved edits, and shows version and filename.
- VST3 project state stores the editor code. Loading a project restores the script in the editor but does not execute user code automatically.
- Python evaluation is isolated from the real-time callback; the callback reads a fixed-size pattern snapshot and generates audio/MIDI timing from the pattern's own tempo.
- A MIDI output event bus is exposed to the host.

## FoxDot-style note cut (0.4.138)

The `cut` player control shortens a note to a fraction of its sustain without
altering its rhythmic step or the next event's position. It accepts scalar,
pattern, `var()` and `linvar()` values in the range 0–1 for both synth and WAV
sample players. A 10 ms release prevents a hard waveform discontinuity.
`Portable/175-foxdot-cut-controls.py` demonstrates changing cut per note and
over time.

## Bounded Sardine-style `@swim` cycle gates (0.4.183)

Inside a bounded `@swim` function, `if every(n)` selects cycle indices
`0, n, 2n, ...`. `if every(n, offset=k)` selects indices whose phase is `k`
within each `n`-cycle group. Both values must be literal integers; `n` is
limited to 1–32 and `offset` to `0..n-1`. The helper is valid only in an
`@swim` conditional. The decorated function is still expanded into the
existing finite event list, so it creates no persistent Python loop or second
scheduler. Example 222 demonstrates a four-cycle melodic variation.

## Variable `@swim` cycle timing (0.4.184)

Cycle timing is already adjustable through `again(function, p=next_period)`.
For a repeated schedule, `@swim(cycles=8, periods=P[0.5, 1, 0.25, 0.75])`
selects the next cycle's period from the finite pattern; `again(function,
i=i+1)` advances it. `periods` accepts a literal pattern/list/tuple of 1–32
values, each 0.001–4096 beats. It cannot be combined with `period=`, and the
expanded timeline remains capped at 4096 beats. Example 223 demonstrates the
shorthand.

Sardine's `@die` stops a named swimming function. The embedded version adds a
bounded stop directive: run `@die` on the same function name to stop the
players still owned by that compiled `@swim`. The stop function body is not
executed. A later ordinary assignment to a player clears that ownership, so
an old `@die` cannot stop a newer unrelated pattern. Example 224 shows the
playable phrase and the separate stop snippet. Because `@swim` is expanded to
a finite event list in the VST, `@die` stops those player schedules; it does
not create or terminate a persistent Python task.

## Beat offsets and finite ramps (0.4.185)

Inside bounded `@swim`, `sleep(0.5)` advances the event cursor by half a beat
for later sends in that cycle; it does not block or sleep the Python worker.
Use a literal duration from 0 to 4096 beats, with at most 32 calls. The
combined sleep offset cannot exceed that cycle's period, and the overall
expanded schedule remains capped at 4096 beats. Example 225 demonstrates a
two-player call-and-response phrase.

`ramp(start, stop, step=1)` returns a finite ascending or descending numeric
pattern, includes the stop value when the step lands on it, and is capped at
256 values. Fractional steps can shape note or duration patterns. Each ramp
value is limited to -1,000,000 through 1,000,000. This is a bounded function
form of Sardine's ramp idea, not its bracket-string pattern syntax. Example
226 demonstrates a pitch ramp and ramped durations.

## Ordered player-effect chains (0.4.137)

`effects=[("Phaser", {"fx_mix": 0.5}), ("Tremolo", {"fx_rate": 5})]`
routes one player's summed stereo signal through the listed inserts from left
to right. Each insert has its own controls and independent smoothed
`var`/`linvar` automation. Up to four inserts per player are accepted; this is
a finite realtime resource bound, not a limit on the installed synth catalog.
The Effects panel shows the chain in order. Examples 173 and 174 demonstrate
pitched notes and drum samples.

This explicit `effects` list is a **Dexter U.S. extension**, not a claim that
FoxDot uses this syntax. It prepares ordered routing for later FoxDot-compatible
effect controls. Existing `effect="Name", fx_*=...` scripts retain their meaning
as one-insert chains. Do not combine `effect=` or standalone `fx_*` controls
with `effects=`. Each chain entry is `(installed_name, controls_dictionary)`;
control names and ranges are the same as the single-effect form. Missing
SynthDefs, malformed entries and more than four inserts produce diagnostics.
`effects=[]` removes the inserts. Stopping a player feeds silence into its
existing chain so delay/reverb state can finish. Changing a chain may reset
the replaced insert's state; a 10 ms de-click transition softens topology
 switches. Unchanged inserts retain their graph state. Optional Airwindows or space-reverb inserts still
require their separately installed UGen packs; stock modulation inserts do not.
No SuperCollider or UGen rebuild is needed for this routing change.

## Optional Limut-inspired inserts (separate UGen pack)

The test branch contains a separate `UGens/LimutFX` pack with two newly
implemented SuperCollider inserts: `LimutPingPong` (stereo cross-feedback
delay) and `LimutDuck` (rhythmic gain pumping). They are adapted from Limut's
`pingpong` and `duck` effect graphs and distributed under CC BY-SA 4.0 with
attribution in the pack. Their timing controls are seconds; at a chosen BPM,
convert beat values with `seconds = beats * 60 / Clock.bpm`.

Install the pack's `LimutFX.scx` into `Portable/SCPlugins` and its two
`.scsyndef` files into `Portable/SynthDefs`, merging those files into the
existing folders, then restart the DAW. The editor/compatibility checker then
offers the effects and their ranged sliders. Example 195 demonstrates both
effects and an ordered two-insert chain. This optional module compiles by
itself against pinned SuperCollider plugin headers; it does not build or bundle
SuperCollider. Other Limut effects that overlap existing inserts are not
duplicated in this pack.

## Per-player modulation inserts (0.4.135–0.4.136)

Chorus, Flanger, Phaser, and Tremolo are per-player inserts loaded from the
bundled `Portable/SynthDefs/*.scsyndef` files. Their precompiled SynthDefs use
stock SuperCollider UGens only; they add no compiled UGen module, third-party
DSP library, or SuperCollider rebuild. Each player has independent effect
state, smoothed `fx_*` controls, and dry bypass. The phaser uses seven cascaded
all-pass stages with bounded feedback; tremolo supports sine/triangle motion
and stereo phase spread. The `effect=` and `fx_*` syntax is a Dexter U.S.
plugin extension rather than stock FoxDot syntax. See Portable Examples
168–172. For source regeneration, run
`python tools/build-modulation-synthdefs.py` with `sclang`; generated
SynthDefs are checked in. The optional audio regression is
`python tools/test-modulation-synthdefs-audio.py`.

## Prototype limits

- Version 0.4.45 rounds both SuperCollider hash-table capacities up to powers of two. The previous graph-definition capacity of 31 violated the pinned hash implementation's bit-mask probing requirement. Compile-time guards and a bounded probing regression test cover this constraint. Lifecycle timing traces remain enabled outside the audio callback; Windows CI preserves validator logs and treats timeout or failure as a workflow failure. Resolution of the host freeze still requires runtime verification.

- SuperCollider is pinned at `ef5771bce3d1c9c780cafd58cf032ec4a3f36d10`; the VST3 SDK checkout is 3.8.1. The Windows script bundles CPython 3.12.10. GitHub Actions builds and validates the Windows VST3; audio, editor, and MIDI behavior still require testing in a real Windows DAW.
  - The engine uses `WorldOptions::mRealTime = false` so SuperCollider does not initialize its own audio driver. One hundred thirty-six fixed voice slots provide 17 slots per player for active notes and overlapping release tails. All four voice modes (sine, triangle, saw, and pulse/square) use the embedded SuperCollider SynthDef and bundled UGen modules. This is not a real-time-safety guarantee for every UGen or third-party module; only load trusted, compatible modules and test them in a DAW. Audit allocations, locks, and thread behavior before treating it as a production instrument.
- Audio pitch follows scheduled MIDI notes in 64-sample SuperCollider blocks, so audio onset is quantized to that block size; MIDI events retain sample offsets. Audio uses raised-cosine 10 ms attacks and up to 40 ms releases for natural note ends and explicit stops, plus smoothed voice-count mix adjustment. The envelope duration is unchanged; only the fade curve is smoother.
- Each player supports up to eight simultaneous notes, and up to eight players can run independently. MIDI is emitted on a separate channel per player. The four starter oscillator modes and fifty-three embedded FoxDot-derived patches are available. Each player can set amplitude, stereo pan, low-pass cutoff, echo, and reverb. FoxDot-style `strum` staggers chord tones; `delay` offsets note starts in beats. Plugin-only `delaytime`, `echomix`, and `feedback` remain distinct extensions. Room/mix uses FreeVerb; the selector also offers FreeVerb2 and GVerb. Portable WAV playback uses the bundled PlayBuf, BufRd, and Phasor UGens for one-shots, normalized start/end slices, and loops bounded by `sus=` or `dur=`. It still does not stream from disk, pitch-preserve time stretch, record, or preserve stereo sample width. The DiskIO UGens and libsndfile are not used by this player. Player aliases beyond eight remain deferred. Unsupported calls and sample-string syntax produce explicit diagnostics instead of silent fallbacks. Arbitrary SynthDef evaluation and DAW transport following are not supported; the editor is Windows-only.
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
