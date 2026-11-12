Dexter U.S. FoxDot - optional JPverb / Greyhole pack (VST >= 0.4.134)

Close the DAW completely, back up your scripts, and MERGE this Portable folder
into the plugin's existing Portable folder. Do not delete Python or other UGens.
SCPlugins contains JPverb.scx and Greyhole.scx; SynthDefs contains both matching
.scsyndef files. Install the modules AND SynthDefs together, then restart the DAW.
Open Example 166 for both effects, or 167 for var/linvar modulation.

These are the original sc3-plugins DSP processors, not approximations. The
per-player stereo insert, dry/wet mix and fx_* Python controls are plugin
extensions. No sclang or external SC server is needed by the installed plugin.
No libsndfile, Ableton Link or additional third-party runtime is introduced.

Controls (finite numbers, var or linvar; invalid values are rejected):
Both: fx_mix 0..1 (0=dry, 1=wet), fx_size 0.5..3,
      fx_moddepth 0..1.
JPverb: fx_t60 0.1..60 seconds, fx_damp 0..0.999,
        fx_earlydiff 0..0.99, fx_modfreq 0..10 Hz.
Greyhole: fx_delay 0.001..1.45 seconds, fx_damp 0..0.99,
          fx_diff 0..0.99, fx_feedback 0..0.98.
Greyhole's modulation frequency is fixed at the upstream default of 2 Hz.
The first implementation permits one persistent insert per player; use separate
players to use JPverb and Greyhole simultaneously. It does not chain two inserts
on the same player. Controls do not overwrite the instrument's own vib/depth etc.

Range note: upstream help advertises size up to 5 and Greyhole delay up to 60,
but the pinned generated DSP clamps size to 3 and delay to 1.45. This API exposes
the actual compiled ranges instead of accepting inaudible out-of-range changes.
Feedback is capped below 1 to avoid intentional infinite sustain.

Memory: either installed SynthDef makes each plugin instance reserve a 256 MiB
SC realtime pool, following both upstream help files. Normal installations
without these SynthDefs retain the previous 64 MiB pool. Removing the optional
SynthDefs and restarting restores that smaller pool. Watch total RAM when using
many plugin instances. Very long tails still use CPU.

Keep the unchanged DSP sources, headers, class definitions, help files, build
instructions and GPL-2.0-or-later notices supplied in Sources and Licenses when
redistributing this pack. Commercial redistribution is allowed under the GPL's
source/distribution requirements.
