# Example 238: tape-head loss. Avoid fast modulation; this UGen can be CPU-heavy.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.06,
            effect="PortedAnalogLossFX", fx_mix=0.25,
            fx_gap=0.25, fx_thick=0.5, fx_space=0.35, fx_speed=0.5)
