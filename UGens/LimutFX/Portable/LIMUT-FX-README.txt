Limut-inspired effect inserts

Install:
  1. Copy SCPlugins/LimutFX.scx into your existing SCPlugins folder.
  2. Copy SynthDefs/LimutPingPong.scsyndef and LimutDuck.scsyndef into
     Portable/SynthDefs.
  3. Keep existing Portable files; merge these additions, do not replace the
     whole Portable folder. Restart the DAW after installing the .scx module.

FoxDot controls:
  LimutPingPong: fx_mix 0..1, fx_time 0.01..2 seconds,
                 fx_feedback 0..0.95.
  LimutDuck:     fx_mix 0..1, fx_period 0.05..8 seconds,
                  fx_depth 0..1, fx_length 0..8 seconds,
                 fx_attack 0.001..8 seconds.

The Duck UGen's period/length/attack controls are seconds at the insert boundary.
To translate Limut's beat values, use seconds = beats * 60 / Clock.bpm.
Both effects work on a player or in an ordered effects=[...] chain. Their
controls accept the embedded var()/linvar() patterns and are range checked.
Example 195 demonstrates both effects and a two-insert chain.

The adapted effects are CC BY-SA 4.0. Preserve ATTRIBUTION.txt when sharing.
