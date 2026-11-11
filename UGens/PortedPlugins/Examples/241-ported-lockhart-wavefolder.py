# Example 241: Lockhart wavefolder with a conservative pregain and wet mix.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.05,
            effect="PortedLockhartWavefolderFX", fx_mix=0.25,
            fx_bias=0.1, fx_pregain=2)
