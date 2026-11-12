# Example 235: PortedPlugins AnalogTape saturation as one isolated insert.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.07,
            effect="PortedAnalogTapeFX", fx_mix=0.35,
            fx_bias=0.5, fx_saturation=0.3, fx_drive=0.25)
