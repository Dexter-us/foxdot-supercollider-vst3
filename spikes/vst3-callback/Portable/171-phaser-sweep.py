# Example 171: a slow phaser sweep moves the sustained chord without a server.
Clock.bpm = 92
Scale.default = Scale.minor

p1 >> saw(P[0, 3, 7, 10], dur=1, sus=0.9, oct=4, amp=0.28,
          effect="Phaser", fx_mix=0.55, fx_rate=0.22,
          fx_depth=0.75, fx_center=var([260, 420, 760], 4),
          fx_spread=0.85, fx_feedback=0.32, fx_tone=10000)

# Sweep the center and rate slowly; feedback remains safely below unity.
p2 >> sine(P[0, 7, 3, 10], dur=2, sus=1.8, oct=3, amp=0.18, pan=0.3,
           effect="Phaser", fx_mix=0.42, fx_rate=0.12,
           fx_depth=0.9, fx_center=linvar([180, 1100], [8, 8]),
           fx_spread=0.65, fx_feedback=0.48, fx_tone=8500)
