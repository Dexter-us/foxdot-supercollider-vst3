# Example 197: compare a standalone pitch shifter with octave-up Shimmer.
Clock.bpm = 92
Scale.default = Scale.minor

p1 >> pluck([0, 3, 7, 10], dur=1, sus=0.8, oct=4, amp=0.2,
            effect="LimutShifter", fx_mix=0.8, fx_ratio=1.5,
            fx_window=0.12)

p2 >> pads([0, 3, 7, 10], dur=2, sus=1.7, oct=4, amp=0.12,
            effect="LimutShimmer", fx_mix=0.55, fx_ratio=2.0,
            fx_window=0.12, fx_room=0.82, fx_damp=0.35)

# The third voice lets you hear the same phrase without either insert.
p3 >> pluck([0, 3, 7, 10], dur=2, sus=1.7, oct=3, amp=0.1)
