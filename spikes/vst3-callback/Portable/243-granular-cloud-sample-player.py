# Example 243: granular cloud from a WAV sample player.
# Install DexterGranularFX.scsyndef in Portable/SynthDefs and reopen the VST.
p1 >> play("x---", sample=0, dur=1, amp=0.24,
          effect="DexterGranularFX", fx_mode=0, fx_mix=0.75,
          fx_density=14, fx_size=0.09, fx_pitch=1.0,
          fx_position=0.45, fx_freeze=0)
