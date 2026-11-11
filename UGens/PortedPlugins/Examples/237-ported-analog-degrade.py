# Example 237: tape wear and degradation with low initial depth/variance.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.07,
            effect="PortedAnalogDegradeFX", fx_mix=0.3,
            fx_depth=0.2, fx_amount=0.2, fx_variance=0.1,
            fx_envelope=0.25)
