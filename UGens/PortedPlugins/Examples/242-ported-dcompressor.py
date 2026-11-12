# Example 242: self-keyed DCompressor. External sidechain routing is not exposed.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 7, 3, 10], dur=1, oct=4, amp=0.06,
            effect="PortedDCompressorFX", fx_mix=0.7,
            fx_ratio=4, fx_threshold=-24, fx_attack=0.01,
            fx_release=0.1, fx_makeup=0.5, fx_automakeup=0)
