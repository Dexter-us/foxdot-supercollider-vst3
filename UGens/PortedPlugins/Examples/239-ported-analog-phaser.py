# Example 239: feedback phaser with an internal LFO.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.07,
            effect="PortedAnalogPhaserFX", fx_mix=0.35,
            fx_rate=0.25, fx_skew=0, fx_feedback=0.25,
            fx_modulation=0.5, fx_stages=8)
