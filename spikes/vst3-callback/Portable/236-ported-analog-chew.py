# Example 236: tape-chew dropouts; keep the depth modest at first.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.07,
            effect="PortedAnalogChewFX", fx_mix=0.3,
            fx_depth=0.2, fx_frequency=0.5, fx_variance=0.1)
