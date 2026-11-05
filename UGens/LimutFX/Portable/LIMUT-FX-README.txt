Limut-inspired effect inserts

Install:
  1. Copy SCPlugins/LimutFX.scx into your existing SCPlugins folder.
  2. Copy the needed files among
     LimutPingPong.scsyndef, LimutDuck.scsyndef,
     LimutDisperser.scsyndef, LimutFuzz.scsyndef, LimutShifter.scsyndef,
      LimutShimmer.scsyndef, LimutGatedVerb.scsyndef, and
      LimutCompressor.scsyndef into
     Portable/SynthDefs.
  3. Keep existing Portable files; merge these additions, do not replace the
     whole Portable folder. Restart the DAW after installing the .scx module.

FoxDot controls:
  LimutPingPong: fx_mix 0..1, fx_time 0.01..2 seconds,
                 fx_feedback 0..0.95.
  LimutDuck:     fx_mix 0..1, fx_period 0.05..8 seconds,
                  fx_depth 0..1, fx_length 0..8 seconds,
                 fx_attack 0.001..8 seconds.
  LimutDisperser: fx_mix 0..1, fx_frequency 30..18000 Hz,
                  fx_q 0.1..10, fx_stages 1..24.
  LimutFuzz:      fx_mix 0..1, fx_drive 0..36 dB,
                  fx_tone 200..16000 Hz.
  LimutShifter:   fx_mix 0..1, fx_ratio 0.5..2, fx_window 0.02..0.5 s.
  LimutShimmer:   fx_mix 0..1, fx_ratio 0.5..2, fx_window 0.02..0.5 s,
                  fx_room 0..1, fx_damp 0..1.
  LimutGatedVerb: fx_mix/fx_room/fx_damp 0..1, fx_threshold 0.001..0.25,
                  fx_hold 0..2 s, fx_attack 0.001..0.25 s,
                  fx_release 0.01..1 s.
   LimutCompressor: fx_mix 0..1, fx_threshold -60..0 dB,
                    fx_ratio 0..20 dB, fx_attack 0..1 s,
                    fx_release 0.001..3 s, fx_knee 0..24 dB.

The Duck UGen's period/length/attack controls are seconds at the insert boundary.
To translate Limut's beat values, use seconds = beats * 60 / Clock.bpm.
Both effects work on a player or in an ordered effects=[...] chain. Their
controls accept the embedded var()/linvar() patterns and are range checked.
Example 195 demonstrates both effects and a two-insert chain.
Example 196 demonstrates Disperser and Fuzz controls and chaining.
Examples 197 and 198 demonstrate Shifter, Shimmer, and GatedVerb and their
player-effect chain. Example 199 demonstrates the compressor. LimutVocoder is
also compiled into LimutFX.scx, but it needs separate carrier and modulator
buses, which the embedded one-player effect insert does not provide yet. It is
not available as effect= or effects=; see Portable/Sidechain/200-limut-vocoder-
two-bus.scd for the standalone routing example. Waveform voices are grouped
separately in the sidebar's Waveforms category.

The pack is CC BY-SA 4.0. Preserve ATTRIBUTION.txt when sharing.
