Dexter U.S. FoxDot portable scripts

Open one of the numbered .py files in this folder from the plugin editor to load a starter pattern. You can edit it, run it, and save your own changes or scripts here. The Open and Save As dialogs default to this folder when the VST3 is installed in a writable location.

06-pattern-tools.py demonstrates reverse, rotate, repeat, transpose, and Euclidean rhythm helpers.
07-independent-players.py shows separate p1 and p2 parts; edit or stop one without replacing the other.

08-synths-and-controls.py demonstrates oscillator choices, per-player amp/pan/cutoff, beat-based echo spacing, and beat-offset delay. MIDI output remains note events.


09-foxdot-core.py demonstrates the initial direct-compatibility subset: Clock.bpm, Scale.default.set(...), p1 >> saw(...), p2 >> pulse(...), cyclic dur values, and per-player oct/sus controls. It uses embedded SuperCollider oscillator UGens and does not start the FoxDot server clock or send OSC. FoxDot sample strings and the wider synth library are not included yet.

10-pattern-transformations.py demonstrates palindrome, stutter, and interleave pattern tools with rests and per-player FoxDot-style saw/pulse syntax.

11-foxdot-pattern-literals.py demonstrates the finite FoxDot-style P[...] literal for note lists, rests, and chords. Slice-based/dynamic patterns and sample strings remain unsupported.

12-pattern-arithmetic.py shows FoxDot-style P[...] numeric transposition with + and -, including laced patterns, rests, and numeric chords.

13-pattern-methods.py demonstrates FoxDot-style transformations directly on finite P patterns: reverse, rotate, variable-count stutter, repeat, and palindrome. The methods return bounded finite patterns and can be used before a player.

14-pattern-composition.py demonstrates FoxDot-style finite phrase concatenation with | and cyclic pattern zipping with &, producing chords while preserving rests.

15-pattern-arithmetic-operators.py demonstrates cyclic *, /, //, %, and ** pattern operations. Zero division resolves to zero like FoxDot; power inputs are bounded for safe finite playback.

16-bounded-pattern-slices.py shows finite P ranges, stepped/descending ranges, and slicing the generated pattern. P ranges need an integer stop and are limited to 256 items.

17-live-player-edits.py demonstrates updating just p1 while p2 continues. Run both assignments once; then edit p1, comment out p2's assignment, and press Run. Unmentioned players keep playing; run p2.stop() alone to stop p2.

The SCPlugins subfolder contains module-loader instructions for adding compatible SuperCollider UGen modules.

18-ui-mouse-controls.py is a Dexter-specific extension: MouseX() pans a player left/right, while MouseY() sweeps another player's cutoff. These wrappers use SuperCollider's UIUGens module; they are not standard FoxDot player controls.

19-envelope-controls.py demonstrates FoxDot-style atk/rel per-voice envelope controls in seconds. The release has a 5 ms minimum in the audio renderer to avoid hard note endings.

20-timevar-controls.py demonstrates var()/linvar() on amp, pan, and cutoff. var steps at beat boundaries; linvar continuously interpolates. Controls are limited to eight numeric breakpoints and run in beats relative to a player assignment.

21-timevar-note-controls.py demonstrates var()/linvar() on dur, oct, sus, and velocity. These are evaluated at each note start; dur changes event spacing, and oct is rounded to an integer octave per note. Event controls are sampled at note boundaries, while amp/pan/cutoff remain audio-rate controls.

22-timevar-effects-envelopes.py demonstrates var()/linvar() on echo spacing, note-start delay, delaytime, echomix, feedback, atk, and rel. Native echo/delay use beat units; delaytime is an explicit plugin seconds override.

23-reverb-algorithms.py keeps FoxDot room/mix syntax on FreeVerb and compares the optional FreeVerb2 and GVerb selectors. GVerb adds revtime and spread controls. Each player owns a separate reverb path.

24-foxdot-filters.py demonstrates the FoxDot-style resonant low-pass (lpf/lpr), high-pass (hpf/hpr), and band-pass (bpf/bpr) filters, with var/linvar sweeps. bpnoise and the separate swell effect are not included yet.

25-echo-decay-compat.py demonstrates FoxDot-style echo spacing and echo-tail decay, both measured in beats; echomix is shown as a separate plugin extension.

26-scales-and-chords.py demonstrates the expanded equal-tempered FoxDot scale bank, custom ascending semitone scales, and chord note groups. Just-intonation and frequency scales are not included.

27-root-and-scale-controls.py demonstrates FoxDot-style Scale.default assignment, Root.default.set(...) using pitch classes or note names, and per-player root overrides.
