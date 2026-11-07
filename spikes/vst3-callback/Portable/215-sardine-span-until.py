# Example 215: Sardine-style span stretches a phrase; until limits its repeats.
Clock.bpm = 104
Scale.default = Scale.minor

# A 4-note phrase is 2 beats long; span=1.5 makes each cycle 3 beats.
# It plays six cycles, then stops scheduling new notes.
p1 >> saw(P[0, 2, 4, 7], dur=0.5, sus=0.42, oct=4,
         span=1.5, until=6, amp=0.2)

# The sample phrase is compressed to half its normal cycle and repeats 8 times.
p2 >> play("x o", dur=0.25, sample=0, span=0.5, until=8,
          amp=0.25, pan=0.2)

# Bounds: span is 0.125..8; until is 1..256 cycles.
# Their combined scheduled horizon is capped at 4096 beats.
# Clock.bpm is unchanged; span changes this player's phrase timing only.
