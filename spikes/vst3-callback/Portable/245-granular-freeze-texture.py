# Example 245: capture the incoming player signal, then freeze the grain source.
# Let it play at least four beats at 120 BPM before fx_freeze rises; lower it
# to record again. At other tempos, allow enough time to fill fx_position.
p1 >> play("x---", sample=0, dur=1, amp=0.20,
          effect="DexterGranularFX", fx_mode=2, fx_mix=0.8,
          fx_density=12, fx_size=0.16, fx_pitch=1.0,
          fx_position=0.45, fx_freeze=var([0, 1], [4, 6]))
