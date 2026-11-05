# Example 199: stereo-linked Limut compressor on a plucked line.
# fx_threshold is dBFS, fx_ratio is the Limut ratio-in-dB control,
# and attack/release are seconds. The detector links both stereo channels.
Clock.bpm = 104
Scale.default = Scale.minor

p1 >> pluck(P[0, 3, 7, 10], dur=0.5, sus=0.42, oct=4, amp=0.36,
           effect="LimutCompressor", fx_mix=1.0, fx_threshold=-24,
           fx_ratio=9, fx_attack=0.008, fx_release=0.14, fx_knee=4)

p2 >> play("x-o-", dur=1, sample=0, amp=0.18, pan=-0.2)
