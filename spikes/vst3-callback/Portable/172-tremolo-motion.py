# Example 172: sine-to-triangle tremolo pulses two independent player parts.
Clock.bpm = 108
Scale.default = Scale.major

p1 >> pluck(P[0, 2, 4, 7], dur=0.5, sus=0.45, oct=4, amp=0.3,
            effect="Tremolo", fx_mix=1, fx_rate=var([4, 6.5], 2),
            fx_depth=0.7, fx_shape=0, fx_spread=0.75,
            fx_phase=0, fx_tone=10000)

# Triangle shape and phase offset make a distinct, alternating stereo pulse.
p2 >> triangle(P[7, 4, 2, 0], dur=1, sus=0.8, oct=3, amp=0.2, pan=-0.25,
               effect="Tremolo", fx_mix=0.85, fx_rate=3.2,
               fx_depth=0.82, fx_shape=var([0.35, 0.9], 4),
               fx_spread=0.9, fx_phase=linvar([0, 0.25], [4, 4]),
               fx_tone=7500)
