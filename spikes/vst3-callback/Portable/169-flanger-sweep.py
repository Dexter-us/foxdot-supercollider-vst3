# Example 169: A slow Flanger sweep makes the sustained synth move.
# Lower fx_feedback (toward 0) for a gentler comb-filter effect.
Clock.bpm = 84
Scale.default = Scale.minor
Root.default.set("D")

p1 >> saw(P[0, 3, 5, 7], dur=2, oct=3, sus=1.8, amp=0.12,
          effect="Flanger", fx_mix=linvar([0.2, 0.55], [4, 4]),
          fx_rate=var([0.12, 0.35], [4, 4]), fx_depth=0.65,
          fx_delay=0.004, fx_spread=0.8, fx_feedback=0.6,
          fx_tone=9000)
