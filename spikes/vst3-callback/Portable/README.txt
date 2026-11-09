0.4.172 replaces the noise-forward JazzKitKick with a warmer low kick and
retains the earlier texture as JazzKitBrushKick. Find the two complete-kit demos
at Portable/217-jazz-kit-core-groove.py and Portable/218-jazz-kit-kick-compare.py.
Together they demonstrate all ten JazzKit voices across two arrangements that
fit the eight-player limit.

0.4.159 adds LimutCompressor as an insert under Dynamics and adds a separate
Waveforms category for sine, saw, pulse and triangle. The LimutFX UGen pack also
contains a true two-input LimutVocoder. It requires separate carrier and
modulator buses, so it is not exposed as a player insert until the embedded
engine has sidechain routing. Example 199 demonstrates the compressor; the
standalone bus example is Portable/Sidechain/200-limut-vocoder-two-bus.scd.

0.4.158 adds three optional compiled LimutFX effect UGens: LimutShifter,
LimutShimmer, and LimutGatedVerb, plus precompiled SynthDefs and Examples 197
and 198. Install LimutFX.scx once under the host's SuperCollider SCPlugins
folder, merge the three matching .scsyndef files into Portable/SynthDefs, then
restart the DAW. Use the Players panel or effect="LimutShifter",
effect="LimutShimmer", or effect="LimutGatedVerb". See
UGens/LimutFX/Portable/LIMUT-FX-README.txt for ranges and the optional-pack
license notes.

0.4.157 adds Limut-inspired LimutDisperser and LimutFuzz effects. It also makes
expanding any Players-panel effect chain select its first insert, so its
effect-specific sliders appear immediately; selecting a later insert switches
to that insert's sliders. LimutDuck is grouped under its own Duck category.
See Example 196.

0.4.156 adds Limut-inspired LimutPingPong and LimutDuck effects. Install the
optional LimutFX.scx and merge the matching SynthDefs; see Example 195 and
UGens/LimutFX/Portable/LIMUT-FX-README.txt.

0.4.155 adds bounded metadata helpers: get_data() returns the live list-backed
view (unwrapping one nested Pattern), get_methods() returns the curated
embedded API names, help() returns text without printing, new(data) creates a
bounded Pattern, make() flattens one nested Pattern in place, and
get_behaviour() returns None. Structural bounds are 256 nodes and 16 levels.
Example 194 demonstrates them.

0.4.154 adds bounded Pattern.true_copy(new_data=None) and
Pattern.convert_data(dtype=float). true_copy makes an independent top-level
list while retaining shared references to nested Patterns and groups; optional
new_data supplies replacement top-level values. The list-backed engine cannot
alias the root list the way Renardo's internal data model does. convert_data
recursively applies an approved pure one-argument converter and preserves
nested structure where possible. Both cap recursion at 16 levels and each
input/output structure at 256 nodes. See Example 193.

0.4.153 adds recursive Pattern.transform(func) for approved pure one-argument
mappers. It transforms leaves inside nested patterns and chord groups, retains
structure, and uses 16-level and 256-node input/output bounds. See Example 192.

0.4.152 adds bounded elementwise ==, !=, <, <=, >, >=, and unary ~ for finite
Patterns. Comparisons return 0/1 patterns, preserve rests, recurse into chord
groups, and cap LCM lacing at 256 results. Unary ~ reverses the top-level and
nested pattern order. Example 191 demonstrates comparison masks and selection.

0.4.151 adds bounded Renardo PZ12 and PFibMod generators. PZ12(tokens, p,
length=256) follows Renardo's documented experimental two-token deficit
ordering; PFibMod(length=31) returns the Fibonacci prefix, capped so notes
remain within the embedded range.
Example 190 combines weighted note selection, a Fibonacci-derived melody, and a
steady sample groove. Callback-based PTree is deferred because arbitrary user
functions are not safe to run in the bounded embedded evaluator.

0.4.150 adds collapsible effect-chain rows in the Players panel. Select an
insert to show only its own controls, current value, and min/max range with
units; dragging a slider changes audio live, and releasing it writes that value
into the selected effects=[...] entry. The compatibility checker now validates
each fx_* control against that specific effect. It also accepts Renardo's
Clock.clear() as a stop-all command for the eight local players and
Seed.default for deterministic random generators (explicit per-generator
seeds take priority). See Examples 187-189.

0.4.142 alphabetizes the instrument list, expands the Effects catalog with the
embedded native controls and optional loaded inserts, and supports right-click
or drag-and-drop from either sidebar onto a player line. A bare `p3 >>` receives
a starter voice; choosing an effect there first creates a default sine player.
Right-click sidebar items for About details and ranges in
179-editor-catalog-about.html. Example 179 demonstrates editing from the panel.
Catalog actions modify source only; click Run to apply. The insert is bounded
to four effects per player.

0.4.141 adds live Players-panel Level, Pan, Cutoff, and FX-mix sliders. Changes
are queued to the local audio engine and smoothed without rerunning the script;
releasing a slider writes its scalar value into the script line for saving.
See Example 178.

0.4.140 adds bounded FoxDot-style `pshift` semitone transposition (-24 to +24)
and native per-player `bits`/`crush` controls. `bits=0` bypasses quantization;
1–24 selects bit depth, and `crush=1–128` reduces the sample-hold rate.
`bits` and `crush` accept scalar, `var()`, and `linvar()` values with smoothing.
`pshift` also accepts finite `P[...]` per-note patterns and transposes WAV
playback as well as pitched synths. The effect runs in the plugin and needs no
external UGen. See Example 177.

0.4.139 adds native FoxDot-style `coarse` pulse gating (0–32) and `formant`
(0–7; use -1 to bypass) controls for synth and WAV players. Both accept scalar,
finite Pattern, `var()`, and `linvar()` values and appear in the live Effects
panel. The built-in Formlet-style resonator approximation needs no external
SynthDef. See Example 176.

0.4.138 adds FoxDot-style note cut:

- `cut` accepts a value from 0 to 1 and ends each note at that fraction of its
  sustain, with a short 10 ms release to avoid a hard discontinuity.
- Scalar, pattern, `var()` and `linvar()` values work on synth and sample players.
- `dur` and the rhythmic event grid stay unchanged; `cut` changes note length.
- Example 175 demonstrates per-note and time-varying cut.

0.4.137 adds ordered player-effect chains:
  effects=[("Phaser", {"fx_mix": 0.5}), ("Tremolo", {"fx_rate": 5})]
Effects run left-to-right; each has independent scalar/var/linvar controls.
Examples 173 (notes) and 174 (drums) use only bundled stock-UGen SynthDefs.
The explicit effects list is a Dexter U.S. extension, not original FoxDot syntax.
Maximum four inserts per player. Do not mix effects= with effect= or standalone
fx_* controls. Existing effect="Name", fx_*=... scripts still work unchanged.

Dexter U.S. FoxDot portable scripts

Version 0.4.148 fixes sidebar insertion for Airwindows Echojam and TapeDelay2:
these effects do not expose fx_mix, so the editor now inserts them without
that control and hides the live FX Mix slider for them. Use their supported
fx_vib/fx_depth/fx_lforate/fx_blur controls (plus TapeDelay2 fx_lfowidth,
fx_rq). The matching compiled SynthDef/UGen pack must still be installed in
Portable/SynthDefs before either insert can be used.

Version 0.4.136 adds per-player Phaser and Tremolo inserts in SynthDefs/.
Examples 171-172 show independent, time-varying settings. Both use stock
SuperCollider UGens; no extra module or SuperCollider rebuild is needed.
Phaser: fx_mix 0-1, fx_rate 0.05-8 Hz, fx_depth 0-1, fx_center 150-1800 Hz,
fx_spread 0-1, fx_feedback 0-0.85, fx_tone 200-16000 Hz.
Tremolo: fx_mix 0-1, fx_rate 0.05-20 Hz, fx_depth 0-1, fx_shape 0-1
(sine to triangle), fx_spread 0-1, fx_phase 0-1, fx_tone 200-16000 Hz.
These effect= / fx_* controls are Dexter U.S. plugin extensions. They accept
scalars, var(), and linvar(); controls are smoothed and ranges are enforced.

Version 0.4.135 adds the stock-UGen Chorus and Flanger per-player inserts.
They are precompiled SynthDefs in SynthDefs/ and need no additional UGen
module. See Examples 168-170 for isolated and combined usage. The controls
effect=, fx_mix, fx_rate, fx_depth, fx_delay, fx_spread, fx_feedback, and
fx_tone are Dexter U.S. plugin extensions, not stock FoxDot controls.
Chorus ranges: mix 0-1, rate 0.05-5 Hz, depth 0-1, delay 0.005-0.04 s,
spread 0-1, feedback 0-0.75, tone 200-16000 Hz. Flanger ranges: mix 0-1,
rate 0.05-4 Hz, depth 0-1, delay 0.0005-0.015 s, spread 0-1, feedback
0-0.95, tone 200-16000 Hz. Control values accept scalars and var()/linvar();
they are smoothed by the player insert. Flanger feedback is bounded to prevent
runaway regeneration.

Version 0.4.134 adds an optional, separately built JPverb/Greyhole UGen pack.
Use effect="JPverb" or effect="Greyhole" with fx_* controls on synth or sample
players. Scalar, var(), and linvar() effect controls are smoothed and kept
separate from instrument controls. These Python insert keywords are plugin
extensions; the actual DSP is the original pinned sc3-plugins code.
Examples 166-167 and the complete control/install guide are in the optional
pack, not this base VST download. Installing either reverb SynthDef reserves
a 256 MiB SC realtime pool per plugin instance, following upstream guidance.
The standard VST workflow still uses the existing cached SC engine.

Version 0.4.133 adds bounded FoxDot `chop` and stereo `spin` controls for synth
and WAV players. `chop` gates the sustain into pulses (0–32); `spin` applies
FoxDot's stereo sine-pan motion (0–40). Both accept scalar, finite P[], var(),
and linvar() controls. See Example 165.

Version 0.4.132 adds FoxDot `vibdepth`, `slide`, `slidefrom`, and `bend`
controls, with `slidedelay`/`benddelay`. Slide and bend follow the bundled
FoxDot SynthDef amplitude envelopes (not pitch glides). Scalar, var()/linvar(),
and finite P[] control values are bounded and applied per note. vib is rate;
vibdepth is depth, sharing the older depth= control (do not specify both).
Ranges: depth 0–10, slide/bend -1–4, delays 0–1. vibdepth is wired to bundled
synths with FoxDot vibrato controls (Example 164 uses viola); basic oscillator
aliases do not yet expose vibrato. See Example 164.

Version 0.4.131 adds native per-player FoxDot-style shape and drive controls.
Both accept numbers, var(), and linvar(), are smoothed to reduce zipper noise,
and work on synth and WAV-player signals. The supported range is 0 to 4;
0 to 1 is the normal FoxDot range, while higher values are stronger. These
self-contained VST DSP curves are not bit-for-bit SuperCollider effect
emulations. See Example 163.

Version 0.4.130 adds a live local-engine browser: Players, Synths, Effects, and
Samples tabs. Player rows show active/muted state, synth, and effect assignments;
the selected player can be muted or stopped. The Samples tab lists Portable
letter-bank WAVs and opens the Samples folder. The compatibility check now
regression-tests .eq(), .getslice(), .i_rotate(), .i_reverse(), .i_sort(),
.i_shuf(), and .setitem() so those supported finite Pattern methods are accepted.
Example 162 demonstrates letter WAV banks, sample= selection, and per-hit |x1|.

Version 0.4.129 adds bounded Pattern .eq()/.ne() comparison masks with cycling,
in-place .i_rotate(), .i_reverse(), .i_sort(), and .i_shuf(), integer
.setitem() mutation, and cyclic .getslice(). Mutators keep Pattern identity so
aliases see edits; sorting rejects incomparable values without partial changes.
Operations are bounded to 256 values and 16 nested levels. See Example 161.

Version 0.4.128 adds bounded FoxDot-style Pattern .getitem(). Integer indices
wrap cyclically, nested finite Patterns advance by cycle, and finite index
Patterns/tuples return bounded Pattern results. Python pattern[index] remains
ordinary non-wrapping list indexing. Inputs/results are limited to 256 items
and 16 nested levels. See Example 160.

Version 0.4.127 adds bounded Pattern .items(), returning a materialized list
of (index, value) pairs. It snapshots nested values and follows the 256-value
and 16-level copy bounds. See Example 159.

Version 0.4.126 adds bounded FoxDot-style Pattern .string(). It joins sample
symbols, formats nested patterns with parentheses and chords compactly, renders
rests as spaces for the embedded play-string parser, and caps output at 256
values, 16 nesting levels, and 4096 characters. See Example 158.

Version 0.4.125 adds FoxDot-style Pattern .set(index, value) and .alt(other).
set mutates the selected item and returns the same Pattern; alt returns a fresh
Pattern without changing its source. Both validate finite numeric/sample data
and respect the 256-item bound. See Example 157.

Version 0.4.124 adds bounded in-place Pattern .insert(index, item),
.pop(index=-1), and .remove(item). Inserted values are validated and copied,
shared references see mutations, and .copy() remains independent. Indexes use
Python list clamping/error behavior; the 256-item cap still applies. See Example
156.

Version 0.4.123 adds in-place Pattern .append(item) and .extend(sequence),
returning None like FoxDot. Shared references to the destination observe the
mutation, while inserted nested values are copied. Only bounded finite
sequences are accepted. Numeric and sample-symbol patterns cannot be mixed;
output is limited to 256 top-level items, nested values to 256, and groups to
8 notes. Examples 154 and 155 demonstrate sharing and independent snapshots.

Version 0.4.122 adds bounded Pattern .str(). Numeric values become text, while
sample symbols and rests are kept unchanged. Numeric text carries an internal
marker, keeping it distinct from playable sample notation and numeric chords.
Nested groups are preserved, with 256-value and 16-level limits. See Example 153.

Version 0.4.121 adds bounded Pattern .int() and .float() conversions. They
recursively convert finite numeric notes/chord members, preserve rests and
nested groups, and do not mutate the source. Inputs are limited to 256 values
and 16 nesting levels; sample strings, booleans, and non-finite values are
rejected. See Example 152.

Version 0.4.120 adds bounded Pattern .asGroup() and .group() aliases. Each
converts a flat finite numeric note pattern into one simultaneous chord, matching
FoxDot. Chords are limited to 8 notes; empty, nested, sample, rest, and
non-finite input is rejected. See Example 151.

Version 0.4.119 adds non-mutating Pattern .copy(). Pattern/group containers
are recursively copied, chord groups keep their type, and the copy is limited
to 256 values and 16 nesting levels. Editing a copied nested pattern does not
change the source. See Example 150.

Version 0.4.118 adds Pattern .every(n, method, *args). It repeats the original
finite phrase n-1 times, then appends one transformed copy. The safe supported
transforms are reverse, rotate, mirror, shuffle, and stutter; output is capped
at 256 items. Arbitrary method names and keyword arguments are rejected. See
Example 149.

Version 0.4.117 adds .limit(func, value) for finite numeric note patterns. It
accepts only the pure built-in sum, max, and len aggregators and caps output at
256 items. Unreachable max thresholds, empty patterns, nested chords, samples,
rests, and non-finite inputs are rejected. See Example 148.

Version 0.4.116 adds bounded non-mutating .concat(other), .add(other), .abs(),
.all(predicate), and .startswith(value) helpers. Concatenation preserves groups
and rejects mixed numeric/sample patterns; predicates must be pure one-argument
functions. Output is capped at 256 items, with numeric groups limited to 8
notes and nesting to 16 levels. See Example 147.

Version 0.4.115 adds bounded .zipx(other) for numeric/chord and sample-symbol
patterns. It cycles both inputs to their LCM length and preserves each zipped
pair's nested structure; sample pairs play as simultaneous layers. Output is
capped at 256 items, nesting at 16 levels, and each hit at 8 notes or symbols.
See Example 146.

Version 0.4.114 adds bounded .duplicate(n) and .iter(n) finite-pattern
repetition. Both preserve nested groups/patterns, accept counts from 0 to 256,
and cap expanded output at 256 items. See Example 145.

Version 0.4.113 adds bounded callable .map(func) and FoxDot-compatible
.submap(mapping). Callable map recursively transforms nested notes and chords;
only pure one-argument arithmetic/conditional functions and selected pure
built-ins are accepted. The existing dictionary .map() behavior is preserved,
including its zero default for unmatched keys. .submap() preserves unmatched
values. Input/output is limited to 256 values and nested patterns to 16 levels.
See Example 144.

Version 0.4.112 adds .sample(n, seed=None) and
.deep_shuffle(n=1, seed=None). Sampling is without replacement by source
position; deep_shuffle creates independently shuffled whole-pattern copies and
keeps nested groups intact. Optional integer seeds from 0 to 4294967295 make
results repeatable. Output is limited to 256 items. See Example 143.

Version 0.4.111 adds finite-pattern .compress(selector) and .select(selector)
mask helpers for numeric and sample-symbol patterns. compress cycles the mask
but never expands beyond the source length; select uses the FoxDot LCM span and
rejects expansions over 256. All-zero masks produce an empty, silent pattern.
Dynamic var/linvar masks are not included. See Example 142.

Version 0.4.110 adds bounded FoxDot-style PSq, PRange, PStep, PEuclid, and
PEuclid2 generators, plus the .norm() and .undup() aliases. The compatibility
check now reports unsupported methods called directly on P[...] before Run.
Portable Example 141 demonstrates numeric sequences and a Euclidean sample
rhythm. Generator output is capped at 256 items.

Version 0.4.109 lets Pvar phrase durations use PRhythm grouped timing and lets
numeric PZip/PZip2 combine chord groups. Grouped PRhythm keeps its base beat
span and bounded offsets while Pvar switches phrases; zipped chords preserve
rests and are limited to 8 notes per event. Pvar remains limited to 8 phrase
states, 256 note slots, and a 4096-beat phrase cycle. See Examples 139–140.

Version 0.4.108 adds Pvar([...phrases...], dur): repeat each finite note or
sample-symbol phrase over its beat span, then switch to the next phrase.
Player dur may be a number, finite numeric pattern, or bounded var/linvar.
Each phrase span must end on a note boundary; the full cycle must return to the
duration-pattern phase, or be an exact multiple of a var/linvar cycle. Limits:
8 phrase states, 256 note slots per player, and a 4096-beat phrase cycle. See
examples 137 and 138.

Version 0.4.107 adds Clock.bpm = var([...], [...]) and linvar([...], [...]).
See Example 136 for stepped tempo, plus a commented continuous-ramp option.
Tempo durations are in beats; the curve continues across shorter player loops.
Run starts the changed players and their tempo curve together, as with the
plugin's existing restart-on-Run behavior (not desktop FoxDot's next-bar clock
assignment). All players in that Run receive the final global BPM setting.
Audio and MIDI triggers use the same tempo map. Delay and sus are converted
using the trigger tempo, matching FoxDot; already-playing WAVs are not stretched.
Other timevar controls follow the changing musical beat. The existing echo's
beat-based time follows tempo, while delay_time and Airwindows times stay in
seconds. Continuous controls are evaluated at the 64-sample SC block rate.
Limits: 1–8 finite BPM values, each >= 0.001; positive durations <= 4096 beats.
There is no added fixed upper BPM ceiling. Existing event/voice capacities
still apply, and acceleration can increase overlapping voices.
This does not add DAW sync, arbitrary live Python callbacks, or time-stretching.

Version 0.4.106 adds bounded PSquare(length=256), producing the sequence
0, 1, 4, 9... as 1–256 values. See Example 135.

Version 0.4.105 adds finite PDelta(deltas, start=0, length=256). It emits the
start value, then accumulates a repeating numeric delta pattern. Scalar deltas
repeat; output is limited to 256 notes and ±1,000,000. See Example 134.

Version 0.4.104 adds finite PChain(mapping, length=256, seed=None). It starts
at the first mapping key, chooses from that state's destinations, and repeats
terminal values. Numeric-note and sample-symbol chains are supported, limited
to 32 states and 256 choices. See Example 133.

Version 0.4.103 adds finite seeded PWalk(max=7, step=1, start=0). It follows
FoxDot's random walk and turning thresholds, supports a cycling numeric step
pattern, and returns 1..256 values. See Example 132.

Version 0.4.102 adds bounded .true_shuffle(n) and .shufflets(n). true_shuffle()
scrambles the expanded finite phrase while preserving chord items; shufflets()
creates seeded shuffled numeric chord groups of up to 8 notes. Output is
limited to 256 items. See Example 131.

Version 0.4.101 adds bounded dictionary .map(mapping) and .pivot(index) pattern
methods. The dictionary form remains available for existing scripts and uses
zero for missing keys. Version 0.4.113 adds FoxDot callable .map(func) and
.submap(mapping), which leaves unmatched values unchanged. pivot() mirrors and
rotates around one or more whole-number steps. Results are limited to 256 values
and 16 nested levels. See Examples 130 and 144.

Version 0.4.100 adds bounded .zip(), .deepzip(), and .deeprzip() pattern
methods. zip() pairs numeric or sample-symbol values into chord-like groups.
The deep forms preserve nested phrase structure; deeprzip() reverses paired
values. Deep output is limited to 256 pairs and 16 levels. See Example 129.

Version 0.4.99 adds FoxDot-style .normalise() for non-empty numeric patterns
and .choose() for one seeded random selection from numeric or sample-symbol
patterns. normalise() maps values to 0..1 and maps constant phrases to zero;
choose() returns one item when the script evaluates. See Example 128.

Version 0.4.98 adds FoxDot-compatible .trim(size) and .ltrim(size) front/tail
slices. Python slice behavior is preserved, including negative sizes; ltrim(0)
returns the full pattern as in FoxDot. See Example 127.

Version 0.4.97 adds bounded finite-pattern .layer("method", ...). It zips
the original phrase with an allowlisted transformed copy as chord groups.
Arbitrary callables are not accepted; results are limited to 256 events.
See Example 126.

Version 0.4.96 adds FoxDot-style finite-pattern .replace(old, new), replacing
matching values inside notes, nested patterns, chords, and sample-symbol
patterns while preserving structure. Nested input is limited to 16 levels.
See Example 125.

Version 0.4.95 adds FoxDot-style finite-pattern .sort() and .unduplicate().
sort() orders comparable values; unduplicate() removes adjacent repeats while
preserving order and later non-adjacent repeats. See Example 124.

Version 0.4.94 adds FoxDot-style finite-pattern .arp(intervals) and
.splice(pattern, ...). arp() arpeggiates numeric notes by adding intervals;
splice() cyclically interleaves numeric phrases or sample-symbol patterns.
Outputs are limited to 256 items. See Example 123.

Version 0.4.93 adds FoxDot's native finite-pattern .invert() method. It reflects
numeric notes around the phrase's highest note, keeps rests, and inverts chord
groups independently. Transposition uses the existing transpose(pattern,
interval) helper; FoxDot does not define .transpose() as a P method. See Example 122.

Version 0.4.92 adds FoxDot-style finite P pattern methods .stretch(size),
.loop(n), and .shuffle(n). stretch repeats to a target length; loop repeats a
whole phrase; shuffle makes independently shuffled copies while keeping chord
items together. Scalar calls are bounded to 256 output items; stretch and loop
also accept finite size/count patterns. See Example 121.

Version 0.4.91 adds bounded finite-pattern `mirror()`, `accum()`, and
`swap(n)` methods. Nested sequences mirror recursively; `accum()` uses
numeric notes and bounded lengths; `swap()` reverses each block. See Example 120.

Version 0.4.90 lets `Root.default.set()` use bounded `var()` and `linvar()`
values. The root is sampled at note onsets; fixed per-player roots override it,
while changing `Scale.default` alone preserves the current root. See Example 119.

Version 0.4.89 adds per-player `root=` and shared `Group.root`. Root changes
take effect at note onsets. `var()` and `linvar()` root values are rounded to
MIDI semitones; fixed roots accept note names or numeric pitch classes/MIDI
notes. See Example 118.

Version 0.4.88 adds bounded `var()` and `linvar()` pitch motion to `Group.degree`.
The contour is sampled and rounded to a scale degree at each note onset (not a
continuous glide); rests remain rests and existing chord intervals are retained.
Values are limited to -128..127; rendered pitches must still resolve to MIDI
notes 0..127. See Example 117.

Version 0.4.87 extends FoxDot-style player groups with shared `degree`, `dur`,
`sus`, `oct`, and `scale` assignments. Pitch controls apply to numeric players;
timing controls also work on sample players. `oct` and `dur` accept `var()` and
`linvar()`. See Example 116.

Version 0.4.86 adds FoxDot-style player groups. `Group(p1, p2)` shares supported
sound/effect controls and `.stop()`, `.mute()`, `.unmute()`, `.solo()`, `.only()`,
and bounded scheduling. `Master()` selects currently active players; `p_all`
contains the eight player slots. Group controls work before or after patterns
and accept `var()`/`linvar()`. See Example 115.

Version 0.4.85 adds FoxDot-style `cycle=` phase offsets and `ident=` schedule
keys to `.every()`, `.often()`, `.sometimes()`, and `.rarely()`. Different
identifiers allow the same transform to run on independent schedules; repeating
the same method/identifier replaces that schedule, while omitting the identifier
retains FoxDot's default replace-by-method behavior. `.every()` uses a scalar
phase within a scalar cycle. The random shortcuts can also use a cycle, compiled
as a seeded four-phase repeating phrase rather than an unbounded random stream.
Sample players may run multiple identified stutter schedules. All operations are
bounded to 256 scheduled calls and 256 output events. The schedule key follows
FoxDot's `Repeatable.every` behavior in `FoxDot/lib/Repeat.py`. See Example 114.

Version 0.4.84 lets numeric players combine distinct scheduled operations, e.g.
`.often("shuffle")` with `.every(1, "stutter", 3)`. The supported operations are
`reverse`, `rotate`, `shuffle`, and `stutter`. See Example 113.

Version 0.4.83 adds scheduled stutter for WAV drum hits; it repeats hits only
when the scheduled beat matches an onset, and skips rests. Retriggers preserve
the chosen sample and are evenly spaced before the next hit. Sample stutter
requires unstrummed hits without per-note delay. See Example 112.

Version 0.4.82 adds `.often()` and `.rarely()` scheduling shortcuts and expands
`.every()`/`.sometimes()` to include seeded degree `shuffle` and bounded
current-note `stutter`. FoxDot 0.6.7 interval ranges are used:
often 0.5–4 beats, sometimes 4–16 beats, and rarely 16–32 beats. As with the
earlier scheduling subset, these are compiled into a bounded repeating phrase;
the final interval may be nudged within the same range to close the phrase.
The embedded Python does not stay running. See Example 111. Stutter accepts
counts 1–16; expanded player phrases are capped at 256 events.

Version 0.4.81 adds live numeric player degree references using
`p2.degree = p1.degree` or `p2.follow(p1)`. When the source player changes in a
script evaluation, followers update their note degrees while retaining their
own synth, rhythm, octave, and mix controls. It also adds bounded player
`.every()` and `.sometimes()` transforms for `reverse` and `rotate`. See
Examples 109–110. These are bounded embedded subsets: scheduled operations
require dense, monophonic numeric note patterns and do not support rests, chords, variable
octaves, strum/delay, or combining a scheduled transform with a player that
follows another. Degree transforms do not alter other controls. `.often()`,
`.sometimes()`, and `.rarely()` use seeded repeating interval phrases.

Version 0.4.79 adds FoxDot-style var()/linvar() modulation to Airwindows
fx_* controls. var() step changes are smoothed with a short 15 ms time constant;
linvar() tracks the player beat clock, with settings applied once per SuperCollider
block. All values remain in the 0..1 range. Example 105 demonstrates both
TapeDelay2 and Echojam beside a dry reference. Existing fixed scalar syntax and
defaults are unchanged.

Version 0.4.78 lets sample players adjust persistent Airwindows inserts using
fx_vib, fx_depth, fx_lforate and fx_blur from 0 to 1 with either
effect="AirwindowsTapeDelay2" or effect="AirwindowsEchojam". TapeDelay2 also
accepts fx_lfowidth and fx_rq. These are fixed scalar settings, applied when
the script is evaluated in that version. The matching SynthDef and UGen pack
must be installed. Example 103 shows all six
TapeDelay2 controls and Example 104 demonstrates Echojam's four controls. Each
example leaves its second player dry for comparison.
Version 0.4.76 adds bounded FoxDot-style PRand and PWhite pattern generators.
PRand chooses from a finite pattern or an inclusive integer range; PWhite uses
FoxDot's symmetric triangular distribution. Both create finite sequences of
1–256 items (default 256), accept seed= for repeatable results, and need no
imports or external server. See Example 102.
Version 0.4.75 lets each sample hit use its own normalized start/end slice,
including P/list patterns and var()/linvar(), and raises the optional master
gain ceiling to 250%. The default remains 100% (unity); see Examples 100–101.
Version 0.4.74 added normalized sample slicing, bounded loop=True playback,
and smooth master output gain. Defaults preserve one-shot samples and unity output.
SAMPLES-SETUP.html explains how to install your own properly licensed WAV files
without bundling the optional demo-sample directory.
Version 0.4.73 fixes startup validation for the bundled Airwindows persistent
effects. Their checked-in SynthDefs use a fixed, tested control-index layout.
Version 0.4.72 added persistent per-player stereo inserts for TapeDelay2 and
Echojam. Install the matching Airwindows pack before Examples 95–98; each
player's notes/chords share one effect state, so delay tails continue between
events and buffers are not allocated per note.

Version 0.4.71 adds external precompiled SynthDef loading. Put one
name-matched `.scsyndef` file per synth in `SynthDefs`; the included
`CustomPluck.scd` is source to compile offline with sclang. The editor call is
`userSynth("CustomPluck", P[0, 2, 4], dur=0.5)`. The embedded engine loads the
binary directly—no sclang process, scsynth server, or OSC. Compatible added
UGen modules belong in `SCPlugins`; install them before using SynthDefs that
depend on them, then restart the DAW. See `SynthDefs/README.txt` and Example 94.
The version 0.4.72 Airwindows SynthDefs are a special case: the VST recognizes
their names as persistent stereo player inserts, uses the player's note voices
as their input, and keeps their effect state across notes. Install the matching
Airwindows UGen pack before using Examples 95–98.

Version 0.4.68 adds FoxDot-compatible Clock.swing(amount=0.1): a sample-accurate
onset nudge alternating every half-beat. Clock.swing(0) disables it. The nudge
is bounded to less than half a beat to avoid negative or wrapped note times.
Its phase starts when a player is run or updated and does not follow DAW
transport. See Example 93.

Version 0.4.67 adds sample-symbol PZip(...) and PZip2(...), matching FoxDot's
cyclic least-common-multiple lacing for finite patterns. The resulting groups
flatten into simultaneous sample hits; a space rests only its own lane. Laced
patterns are limited to 256 items and hits to eight symbols. PZip2 accepts an
optional two-value rule. Numeric PZip behavior is unchanged; see Example 92.
OPTIONAL-SAMPLE-SETUP.html explains how to install separately downloaded WAVs.
Version 0.4.66 adds finite sample-symbol P & P chord-zipping. The patterns cycle
to their least-common-multiple length (up to 256 steps); each event can play up
to eight sample symbols, and a space rests only that layer. See Example 91.
Version 0.4.65 adds grouped sus= values using PGroup(...), laced with chord
tones and PRhythm offsets. Grouped events count toward the eight-voice and
256-event limits; see Example 90. Version 0.4.64 added nested PRhythm duration
groups, flattened to bounded note offsets with nesting limited to eight levels;
see Example 89.
Version 0.4.63 extends PAlt, PStretch, PStutter, PShuf, and PJoin to bounded
numeric note patterns, rests and chord groups as well as sample-symbol inputs.
PStutter cycles source and count patterns to their least-common-multiple length,
matching FoxDot. Example 88 demonstrates the numeric forms.

Version 0.4.62 supports overlapping sustains (`sus > dur`) and repeated pitches
in FoxDot-style players. Voice overlap is bounded to eight simultaneous notes
per player, with sus capped at eight pattern cycles. Example 87 demonstrates
overlap. Version 0.4.61 adds bounded numeric-note PZip chord pairing and PZip2 pairing
with a rule; see Example 86. Numeric PZip/PZip2 accept 1D note patterns and
rests, capped at 8 inputs and 256 laced items. Version 0.4.67 adds sample-symbol
patterns; nested numeric chords remain unsupported.
Version 0.4.60 adds PQuicken delay clusters and a bounded PRhythm subset;
see Example 85. PRhythm accepts positive durations and
(n, k[, start[, dur]]) Euclidean tuples. A tuple group in dur= plays delayed
repeats at the cumulative Euclidean offsets. Nested lists create bounded groups
using cumulative child-duration offsets. Grouped sus= accepts PGroup(...) and
laces sustain values across chord, duration-group and sustain-group lengths;
shorter groups cycle to the longest.
PQuicken output and each player are capped at 256
expanded note events; delay offsets are limited to 8 beats.
Version 0.4.59 adds bounded PDelay groups and the FoxDot PStrum rhythm
pattern; see Example 84. A PDelay group repeats each note at its cumulative
beat offsets and is limited to 256 expanded note events per player.
Version 0.4.58 matches FoxDot's slice-based `start` rotations for PBeat and
PDur, including negative and over-length offsets; see Example 83.
Version 0.4.57 adds bounded FoxDot-compatible `PBeat(...)` and `PDur(...)`
duration patterns.
Version 0.4.56 adds sample-symbol `PShuf(...)` and `PJoin(...)`; see Examples 81–82.
Version 0.4.55 added sample-symbol `PStretch(...)` and `PStutter(...)`; see Example 80.
Version 0.4.54 added sample-symbol `PAlt(...)` alternation; see Example 79.
Version 0.4.53 added FoxDot-style `|` concatenation for sample-symbol P patterns;
see Example 78. Version 0.4.52 added finite sample-symbol sequences and
transforms; see Example 77. Version 0.4.51 added per-hit sample-index selectors;
see Example 76.
The 0.4.46 synth batch has one isolated synth test per file, examples 43-68:
43 viola, 44 nylon, 45 arpy, 46 dab, 47 sawbass, 48 dbass,
49 prophet, 50 pads, 51 pasha, 52 ambi, 53 sinepad, 54 keys,
55 klank, 56 feel, 57 glass, 58 soft, 59 quin, 60 space,
61 scratch, 62 creep, 63 snick, 64 swell, 65 razz, 66 sitar,
67 star, 68 karp.
They use original FoxDot core-UGen graphs, not samples or MdaPiano.
Load one at a time initially; each uses p1, so running the next replaces it.
Updated examples 43-68 explicitly stop p2-p8 before starting p1. Without that,
players from an earlier multi-player demo can keep sounding underneath the
new test. These are deliberately sparse four-note checks, not finished tunes.
The swell() synth is different from the existing swell= player effect.
69-composed-star-and-karp.py is a different, arranged example: concatenated,
rotated and transposed lead phrases, a reversed response, and a quiet sine
reference. It needs the 0.4.47 fresh-note startup fix.

Open one of the numbered .py files in this folder from the plugin editor to load a starter pattern. You can edit it, run it, and save your own changes or scripts here. The Open and Save As dialogs default to this folder when the VST3 is installed in a writable location.

06-pattern-tools.py demonstrates reverse, rotate, repeat, transpose, and Euclidean rhythm helpers.
07-independent-players.py shows separate p1 and p2 parts; edit or stop one without replacing the other.

08-synths-and-controls.py demonstrates oscillator choices, per-player amp/pan/cutoff, beat-based echo spacing, and beat-offset delay. MIDI output remains note events.


09-foxdot-core.py demonstrates Clock.bpm, Scale.default.set(...), p1 >> saw(...), p2 >> pulse(...), cyclic dur values, and per-player oct/sus controls. It uses embedded SuperCollider oscillator UGens and does not start the FoxDot server clock or send OSC.

10-pattern-transformations.py demonstrates palindrome, stutter, and interleave pattern tools with rests and per-player FoxDot-style saw/pulse syntax.

11-foxdot-pattern-literals.py demonstrates the finite FoxDot-style P[...] literal for note lists, rests, and chords. It is separate from the direct play-string syntax in examples 70-75.

12-pattern-arithmetic.py shows FoxDot-style P[...] numeric transposition with + and -, including laced patterns, rests, and numeric chords.

13-pattern-methods.py demonstrates FoxDot-style transformations directly on finite P patterns: reverse, rotate, variable-count stutter, repeat, and palindrome. The methods return bounded finite patterns and can be used before a player.

120-pattern-shaping-helpers.py demonstrates nested mirror(), cumulative degree contours with accum(), and reversed blocks with swap().

121-pattern-method-parity.py demonstrates method-form stretch(size), loop(n),
and shuffle(n) on finite numeric, chord, and note patterns.

122-pitch-inversion.py demonstrates .invert() on a numeric phrase and
transpose(pattern, interval) on chords and notes.

123-arpeggio-and-splice.py demonstrates .arp() and .splice() on note
patterns, including chord intervals.

124-sort-and-unduplicate.py demonstrates ascending pattern sorting and
consecutive-only duplicate removal.

125-pattern-replace.py demonstrates replacing note degrees, chord tones, and
sample symbols.

126-pattern-layer.py demonstrates reverse and rotate pattern layers.

127-pattern-trim.py demonstrates splitting a phrase into front and tail parts.

128-normalise-and-choose.py demonstrates a normalized pitch contour and a
selected pitch value.

129-pattern-zip-methods.py demonstrates paired dyads and nested zip order.

130-pattern-map-pivot.py demonstrates dictionary note mapping and rotating/
mirroring a phrase around selected steps.

131-true-shuffle-and-shufflets.py demonstrates expanded seeded shuffling and
shuffled chord passes with strum.

132-pwalk-melodic-walk.py demonstrates a seeded melodic walk, a slower bass
walk, and drums.

133-pchain-transition-melody.py demonstrates numeric transition chains and a
sample-symbol drum chain.

134-pdelta-pitch-contour.py demonstrates interval accumulation in a lead and
bass contour.

135-psquare-pitch-sequence.py demonstrates a square-number pitch pattern.
136-variable-bpm.py demonstrates stepped BPM and an alternative linvar ramp.
137-pvar-phrase-switching.py switches a lead phrase and a sample-symbol drum
phrase on timed boundaries, using alternating lead note durations.
138-pvar-timevar-durations.py demonstrates var/linvar note durations inside
bounded Pvar phrases. 139-pvar-grouped-rhythm.py combines Pvar phrase changes
with PRhythm grouped durations. 140-numeric-pzip-chord-groups.py joins numeric
chord groups with PZip and PZip2.

14-pattern-composition.py demonstrates FoxDot-style finite phrase concatenation with | and cyclic pattern zipping with &, producing chords while preserving rests.

15-pattern-arithmetic-operators.py demonstrates cyclic *, /, //, %, and ** pattern operations. Zero division resolves to zero like FoxDot; power inputs are bounded for safe finite playback.

16-bounded-pattern-slices.py shows finite P ranges, stepped/descending ranges, and slicing the generated pattern. P ranges need an integer stop and are limited to 256 items.

17-live-player-edits.py demonstrates updating just p1 while p2 continues. Run both assignments once; then edit p1, comment out p2's assignment, and press Run. Unmentioned players keep playing; run p2.stop() alone to stop p2.

70-73 demonstrate portable WAV playback, sample/rate controls and var()/linvar().
100-sample-loop-and-slicing.py demonstrates fractional start/end positions and
a loop limited by sus= (or dur= when sus is omitted).
101-patterned-sample-slices.py demonstrates start/end values that cycle per hit
or vary with var()/linvar(); every emitted hit is checked to ensure
0 <= start < end <= 1. loop= remains a fixed Boolean control.
102-random-pattern-generators.py demonstrates seeded PRand pitches and PWhite
durations. PWhite uses the same triangular distribution as FoxDot; values are
generated once per script run and the finite result cycles in the player.
Re-run the script for a fresh unseeded sequence.
103-sample-player-airwindows-delay.py applies TapeDelay2 to the sliced p1 WAV
player while leaving p2 dry. Edit its fx_vib, fx_depth, fx_lforate,
fx_lfowidth, fx_rq and fx_blur values (each 0..1), then re-run the script.
104-sample-player-airwindows-echojam.py demonstrates Echojam using fx_vib,
fx_depth, fx_lforate and fx_blur (each 0..1). Both require the matching
Airwindows SynthDef in Portable/SynthDefs and its UGen module in Portable/SCPlugins.
74-nested-play-strings.py demonstrates `[]` step subdivisions and `()` lacing.
75-random-and-layered-strings.py demonstrates bounded `{}` variations and
adjacent `<>` layers. The compatibility checker reports malformed strings;
76-sample-index-selection.py demonstrates FoxDot's `|x1|` notation to choose
one sample variant for a single hit. Example 77 uses `P["x", "o", " "]` sample
sequences with `.reverse()` and `.rotate(n)`; the values must be single sample
symbols or space-rests, and other hits still use `sample=`.
78-sample-pattern-concatenation.py demonstrates left-to-right sample-pattern
concatenation with `|`, followed by rotation. Numeric P arithmetic over
sample-symbol patterns remains unsupported.

79-sample-pattern-alternation.py demonstrates PAlt cycling patterns to their
least-common-multiple length, then alternating one item from each input.
PAlt also supports numeric note patterns in 0.4.63.

80-sample-pattern-stretch-stutter.py combines PStretch, which cycles a source
pattern to the requested length, with PStutter, which repeats each symbol by
the matching count. PStutter count patterns cycle with the source to their
least-common-multiple length, matching FoxDot. Each input, iteration count and
expanded result is limited to 256 items; repeat counts are whole numbers 0–16.
Both helpers also accept numeric notes, rests and chord groups in 0.4.63.

81-sample-pattern-shuffle.py uses PShuf to randomly reorder a finite sample
pattern while preserving all hits and rests. It uses an internal Fisher-Yates
shuffle with a per-evaluation seed and adds no third-party dependency. PShuf
also accepts numeric note patterns and preserves their rests/chords.

82-sample-pattern-join-shuffle.py joins smaller motifs left-to-right with
PJoin, then shuffles the complete phrase with PShuf. Both helpers accept numeric
phrases as well as sample-symbol phrases.

92-sample-pzip-pzip2.py combines finite sample-symbol patterns with PZip and
filters zipped sample pairs with PZip2. Unequal patterns cycle to their LCM;
spaces rest their own lane and groups are limited to eight simultaneous hits.

83-pbeat-pdur-rhythm-durations.py uses PBeat pulse strings and PDur Euclidean
durations to control the timing of portable sample patterns.

84-pdelay-pstrum-combination.py combines PDelay's cumulative beat offsets with
the PStrum rhythm pattern on a melodic player.

85-pquicken-prhythm.py demonstrates a Euclidean duration group on sample
patterns and a PQuicken delay cluster on a melodic player.

86-pzip-pzip2-chords.py demonstrates PZip's rotating chord combinations and
PZip2's filtered chord pairs.

87-overlapping-sustain-voices.py demonstrates same-pitch retriggers and chord
voices sustaining over a pattern loop.

88-numeric-pattern-transforms.py uses PAlt, PStretch, PStutter, PShuf, and
PJoin on scale-degree notes, including numeric rest/chord support.

89-nested-prhythm.py demonstrates nested duration groups, including a nested
Euclidean tuple and the resulting retrigger offsets.
90-grouped-sustain.py demonstrates grouped per-voice sustain on chord tones and
the bundled sample player.

91-sample-symbol-chord-zip.py uses FoxDot's `&` operator to zip finite sample
patterns into simultaneous hits. Unequal pattern lengths cycle to their
least-common-multiple; spaces rest only their own layer.

The SCPlugins subfolder contains module-loader instructions for adding compatible SuperCollider UGen modules.

18-ui-mouse-controls.py is a Dexter-specific extension: MouseX() pans a player left/right, while MouseY() sweeps another player's cutoff. These wrappers use SuperCollider's UIUGens module; they are not standard FoxDot player controls.

19-envelope-controls.py demonstrates FoxDot-style atk/rel per-voice envelope controls in seconds. The release has a 5 ms minimum in the audio renderer to avoid hard note endings.

20-timevar-controls.py demonstrates var()/linvar() on amp, pan, and cutoff. var steps at beat boundaries; linvar continuously interpolates. Controls are limited to eight numeric breakpoints and run in beats relative to a player assignment.

21-timevar-note-controls.py demonstrates var()/linvar() on dur, oct, sus, and velocity. These are evaluated at each note start; dur changes event spacing, and oct is rounded to an integer octave per note. Event controls are sampled at note boundaries, while amp/pan/cutoff remain audio-rate controls.

22-timevar-effects-envelopes.py demonstrates var()/linvar() on echo spacing, note-start delay, delaytime, echomix, feedback, atk, and rel. Native echo/delay use beat units; delaytime is an explicit plugin seconds override.

23-reverb-algorithms.py keeps FoxDot room/mix syntax on FreeVerb and compares the optional FreeVerb2 and GVerb selectors. GVerb adds revtime and spread controls. Each player owns a separate reverb path.

24-foxdot-filters.py demonstrates FoxDot-style resonant LPF/HPF/BPF controls, band-pass `bpnoise` movement, and the envelope-shaped high-pass `swell` effect.

25-echo-decay-compat.py demonstrates FoxDot-style echo spacing and echo-tail decay, both measured in beats; echomix is shown as a separate plugin extension.

26-scales-and-chords.py demonstrates the expanded equal-tempered FoxDot scale bank, custom ascending semitone scales, and chord note groups. Just-intonation and frequency scales are not included.

27-root-and-scale-controls.py demonstrates FoxDot-style Scale.default assignment, Root.default.set(...) using pitch classes or note names, and per-player root overrides. 119-global-root-timevars.py demonstrates session-wide stepped var() and semitone-gliding linvar() root changes.

28-roman-numeral-chords.py demonstrates FoxDot's I–VII triad and seventh-chord names, plus custom chords made with PGroup(...).

29-strummed-chords.py demonstrates chord-note staggering with strum in beats, including a time-varying strum value. Its sus=1.8 controls the note tails; overlaps into following chord notes are valid.

29-strummed-chords.py uses a moderate 0.04-beat spread at 84 BPM, with a softer second layer whose strum varies over time. Strum is spacing per chord tone; larger spreads create more overlap, up to the player's eight-voice safety limit.

31-bass-varsaw-synths.py tests the embedded FoxDot bass and variable-width saw patches with explicit softer note releases; no sample files or separate server are used.
32-dirt-jbass-noise.py pairs a clearer jbass riff with quieter dirt and noise accents. Dirt and noise are intentionally rougher than the stable bass voices.

33-dub-growl-soprano.py and 34-crunch-lazer-rave.py test the second FoxDot synth batch, including Gendy1.
35-resonant-bells.py demonstrates bell, gong, and marimba using the bundled Klank resonator.
36-plucked-resonators.py demonstrates the pluck, twang, donk, and squish resonator patches.

37-bandpass-motion-and-swell.py demonstrates bpnoise modulation of the band-pass center and resonance, plus the swell high-pass envelope effect.
38-bass-varsaw-release-check.py compares dry bass with reverberated varsaw using matching, slow notes. The engine keeps a short silent graph tail so the reverb send fades before the voice graph stops.
39-foxdot-native-bass-varsaw.py omits Dexter's atk/rel overrides so bass uses FoxDot's 20 ms attack plus sus-length decay and varsaw uses its linear sus/2–sus/2 envelope.
40-charm-and-spark.py demonstrates two more self-contained FoxDot patches with their source-style resonant sweep and amplitude motion.
41-blip-oscillator.py demonstrates FoxDot's LFCub/LFTri/Blip texture; rate controls the Blip harmonic count.

42-self-contained-ugens.py introduces scatter, orient, zap, fuzz, bug, and ripple. These first six additions use standard SuperCollider UGens and require no extra plugin modules.
30-player-mute-controls.py shows p2.mute() and p2.unmute(); the comment markers are at column one so uncommenting a command does not introduce Python indentation.

141-finite-pattern-generators.py demonstrates FoxDot-style PSq, PRange, PStep, PEuclid, and PEuclid2. The generated patterns are bounded to 256 items; PEuclid2 can create a sample-symbol drum pattern with space-rests.

0.4.142 adds the sorted instrument catalog, expanded Effects tab, right-click
Insert/About actions, and drag-and-drop onto player lines. Example 179 shows the
live catalog workflow.

0.4.143 groups Synths and Effects by family. It also adds player-specific Bits
and Crush sliders: select an active player using `bits=` or `crush=` to show
them; changes are smoothed into the running sound, and releasing a slider
writes its value back into that player call. Example 180 demonstrates it.

0.4.144 adds bounded P10, PBern, PSine, PTri, and PSum generators. Random
generators accept an optional seed for repeatable results. The compatibility
checker now allows `.expand()` on Pvar phrases, not on ordinary finite P
patterns. Selecting any active player exposes a PShift semitone slider; audio
voices retune smoothly, while MIDI output applies the changed shift to later
note-ons. Releasing writes a scalar value back into the player call. Synths and
Effects category headings can be clicked to collapse or expand their entries.
Examples 181 and 182 demonstrate these changes.

0.4.147 adds bounded Pattern.flatten(), which turns one Pattern level into a
simultaneous numeric PGroup chord, with an eight-note maximum. Example 185
demonstrates it. The earlier checker errors for eq, getslice, i_rotate,
i_reverse, i_sort, i_shuf, and setitem are already supported in this version;
if they still appear, confirm the loaded plugin version.

0.4.146 keeps all player controls visible (but disabled) before selection,
including PShift, and clicking a `pN >>` code line selects that player in the
Players panel. Example 184 demonstrates pitch-shift and bitcrusher controls.

0.4.145 adds finite PIndex(length), seeded PxRand choices without adjacent
repeats, and seeded PwRand weighted choices. Inputs and output patterns are
bounded to 256 items; weights must be whole numbers in the supported range.
Example 183 demonstrates numeric and sample-symbol choices. Synth and Effect
catalog family headings now start collapsed; click a heading to expand it.

0.4.173 keeps custom user-SynthDef slider overrides when Run recompiles the
same player and synth; choosing another synth restores that synth's controls
sidecar defaults. This is session-only and does not rewrite the script.

0.4.174 adds a bounded Sardine-style `@swim` subset. The editor expands at most
32 cycles and 256 events per player into a finite pattern, with a 4096-beat
timeline limit. `again()` may advance `p` and `i`; loops, nested decorators,
and dynamic synth/effect changes are not supported. Example 219 demonstrates
cycle-by-cycle note/sample variation.

0.4.175 adds bounded `if`/`else` branches inside `@swim`, driven only by
integer expressions based on the cycle index `i`. Comparisons, `and`/`or`,
`not`, and bounded integer arithmetic are supported. This enables cycle-based
fills and early endings without exposing arbitrary Python execution. Branch
expressions are limited to 32 AST nodes and 8 levels; the function is limited
to 32 conditional statements, four nested levels, and the existing cycle,
event, and timeline caps. Example 220 demonstrates a four-beat fill.


0.4.177 adds a separate SuperCollider (.scd) editor tab backed by the pinned
SuperCollider 3.15 sclang language runtime. It connects by private loopback OSC to
the VST's existing embedded SuperCollider World; it does not start a second
scsynth/audio engine. The FoxDot/Python tab remains separate and keeps its
bounded compatibility checker. Example 222 is a starter score for the .scd tab.
The sclang-side audio-bus allocator reserves the buses already used by VST
voices and effect chains so ordinary Bus.audio allocations do not collide.

The SuperCollider tab executes full language code, including file and process
operations. Run only source you trust. The helper runs headlessly and is built
with SCLANG_SERVER=OFF. Portable/SCPlugins contains only modules built for the
same pinned SuperCollider 3.15/API-7 engine; the former SC3.14.1/API-3
AntiAliasingOscillators probe was removed because it was incompatible with
this VST.

0.4.178 corrected the bootstrap's Server variable, but its native TCP dispatch
still required an audio driver that a host-driven NRT World does not have.
0.4.180 replaces that network path; no second audio server is started.

0.4.179 defers embedded-engine startup until the first Run. Initial host audio
setup records the sample rate and returns without scanning/loading the engine.
Idle audio is silent and successful while the engine is absent or preparing.
Both Python and SuperCollider engine preparation use a background script worker.
Installed SynthDef names are discovered separately so the sidebar still fills
before first Run. The SC editor document is created on first tab selection.
Closing on the SC tab and reopening no longer mixes the Python and SC sources.
The editor and Run button have distinct child IDs, and the DLL-owned editor
window class is released when the last editor closes and re-registered on reopen.

Windows lifecycle diagnostics are written to
%TEMP%\Dexter-VST-lifecycle-<host-process-id>.log (phase/timing/errors only;
no script contents). Files are bounded to approximately 2 MB per host process.
The Windows checks now include actual editor open/switch/reopen tests and an
actual VST3 first-Run audio check, in addition to the standard VST3 validator.
These checks do not substitute for testing the editor and sclang in your DAW.

0.4.180 adds a bounded host-driven OSC bridge for the language tab. It does not
use SC's hardware-driver network queue, and status replies do not dereference an
absent hardware driver. A loopback UDP worker performs validated NRT commands
under the engine mutex; socket waits and SynthDef loading stay off the UI/audio
threads. Only one private language client is supported. No SC/UGen rebuild is
needed for this change.

Normal SC output buses 0/1 now enter the VST mix and master gain, only when
written in the current block. SC Stop/CmdPeriod clears the language's group,
not FoxDot's cached voice/effect nodes. Language node IDs start at 16777216;
private VST IDs stay below that range. Bundled SynthDefs cannot be overwritten
by language definitions. Failed/missing commands return /fail, not a silent
promise of full-server compatibility.

Initial server command subset:
status, notify, version, sync, clearSched, d_recv (SynthDef.add), s_new,
n_set, n_run, n_free, g_new, g_freeAll and g_deepFree. Both string and numeric
SC command forms are supported. Tree insertion supports head/tail, not
replacing/reordering FoxDot's nodes. d_recv accepts an absent/empty completion
message, not nested completion commands. Buffer/disk/recording/mapping/query
commands and external hardware-server control are not part of this bridge yet.
The 256 distinct language SynthDef names include SC's automatic system defs.
OSC datagrams are at most 65504 bytes with 512 arguments; controls are finite
scalar int32/float32 values. Queues hold at most 256 messages/1 MiB;
bundles nest at most eight levels and schedule at most 30 seconds ahead.
Timed bundles dispatch at host-block boundaries, not sample-accurately.

Startup is limited to 20 seconds and Stop can cancel a pending language launch.
Windows owns sclang in a kill-on-close job, inheriting only its input/log
handles; closing/crashing the host must not leave the language helper running.
Example 223 tests SynthDef.add, sync, a stereo tone, control changes and release.
The required Windows test now starts the bundled interpreter, requires actual
SC-tab audio, runs Python alongside it, and checks that SC Stop preserves Python
while removing the SC test tone. Windows/DAW results remain pending until run.
The same check exits/restarts sclang and requires renewed SC audio. A new
language client's registration retires the old client/group/scheduled events,
so a stale UDP notification login cannot permanently block retries.

0.4.181 moves the bundled bass and jbass demo lines out of the subsonic range.
FoxDot's original Bass/JBass definitions divide oscillator frequency by four;
the examples now use the normal octave-5 register. This changes example
settings only, not the original SynthDefs or compatibility behavior.

0.4.182 supports the existing Sardine-style chance, loaf and on controls inside
finite @swim phrases. Each player's chance seed is held constant while its
cycles are expanded; Seed.default still provides a repeatable seed between
runs. Example 221 demonstrates a 50% chance on bars 1 and 3 of a four-bar
loaf, alongside an "often" drum part gated to every second bar.
