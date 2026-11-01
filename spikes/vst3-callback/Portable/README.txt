Dexter U.S. FoxDot portable scripts

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

27-root-and-scale-controls.py demonstrates FoxDot-style Scale.default assignment, Root.default.set(...) using pitch classes or note names, and per-player root overrides.

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
