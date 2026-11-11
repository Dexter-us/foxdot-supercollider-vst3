# Example 244: stepped granular slices from the sample player's audio.
# fx_density is grains/second; BPM=120 gives two quarter-note grains/second.
p1 >> play("x-o-", sample=0, dur=0.5, amp=0.22,
          effect="DexterGranularFX", fx_mode=1, fx_mix=0.8,
          fx_density=2, fx_size=0.12, fx_pitch=1.0,
          fx_position=0.1, fx_freeze=0)
