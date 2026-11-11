# Example 246: granularize a live player voice (not an external audio input).
# This works on a synth player or on play(...) routed through the same insert.
p1 >> pluck(P[0, 3, 7, 10], dur=0.5, amp=0.16, oct=4,
            effect="DexterGranularFX", fx_mode=3, fx_mix=0.7,
            fx_density=10, fx_size=0.08, fx_pitch=1.25,
            fx_position=0.5, fx_freeze=0)
