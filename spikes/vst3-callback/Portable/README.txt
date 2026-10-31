Dexter U.S. FoxDot portable scripts

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
