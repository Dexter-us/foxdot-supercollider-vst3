# Example 240: Baxandall-style EQ and vintage distortion; oversampling is fixed off.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.06,
            effect="PortedAnalogVintageDistortionFX", fx_mix=0.3,
            fx_drivegain=0.25, fx_bias=0, fx_lowgain=0.08,
            fx_highgain=0.08, fx_shelvingfreq=600)
