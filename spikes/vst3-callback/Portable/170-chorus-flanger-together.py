# Example 170: Hear both inserts together; each player has its own effect.
Clock.bpm = 90
Scale.default = Scale.minor
Root.default.set("C")

p1 >> pluck(P[0, 2, 4, 7], dur=1, oct=4, sus=0.85, amp=0.16,
            effect="Chorus", fx_mix=0.3, fx_rate=0.7,
            fx_depth=0.35, fx_delay=0.02, fx_spread=0.8)
p2 >> triangle(P[4, 2, 1, 5], dur=1, oct=5, sus=0.7, amp=0.1,
               effect="Flanger", fx_mix=0.35, fx_rate=0.25,
               fx_depth=0.55, fx_delay=0.003, fx_spread=0.7,
               fx_feedback=0.45, fx_tone=8500)
