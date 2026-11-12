# Example 174: sampled drums -> Flanger -> Chorus; bass remains dry.
# Requires the bundled drum WAVs and stock-UGen Flanger/Chorus SynthDefs.
Clock.bpm = 110
Scale.default = Scale.minor

p1 >> play("x-o-xxo-", dur=0.25, sus=0.22, sample=0, amp=0.26,
           effects=[
               ("Flanger", {"fx_mix": 0.55, "fx_rate": 0.25,
                            "fx_depth": 0.65, "fx_feedback": 0.35}),
               ("Chorus", {"fx_mix": linvar([0.2, 0.65], [4, 4]),
                           "fx_rate": 0.9, "fx_depth": 0.4,
                           "fx_spread": 0.9})
           ])

p2 >> sine(P[0, 0, -2, 3], dur=1, sus=0.6, oct=3, amp=0.18,
           atk=0.01, rel=0.12)
# p1.stop() stops incoming hits; the insert chain continues its remaining tail.
