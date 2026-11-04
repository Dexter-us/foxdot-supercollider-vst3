# Example 175: FoxDot-style cut shortens each note without changing the beat.
Clock.bpm = 105
Scale.default = Scale.minor

# Each saw note sounds for a different fraction of its sustain.
p1 >> saw(P[0, 3, 5, 2], dur=1, sus=0.9,
          cut=P[1.0, 0.75, 0.5, 0.35], oct=4, amp=0.24)

# The sample player's cut changes over time too; rests stay silent.
p2 >> play("x-o-", dur=0.5, sample=0, sus=0.45,
          cut=var([0.8, 0.35], 1), amp=0.32, pan=-0.2)
