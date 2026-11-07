# Example 201: SCLOrk-derived stereo inserts use stock SuperCollider UGens.
# Install the three .scsyndef files from Portable/SynthDefs and restart the DAW.
Clock.bpm = 100
Scale.default = Scale.minor

# Phazer: moving BAllPass phase cancellation on the incoming stereo player bus.
p1 >> pluck(P[0, 3, 7, 10], dur=1, sus=0.8, oct=4, amp=0.22,
            effect="SCLOrkPhazerFX", fx_mix=0.45, fx_rate=0.22,
            fx_depth=0.65, fx_center=650, fx_spread=0.8,
            fx_q=0.25, fx_tone=11000)

# Decimator: lower fx_rate and fx_bits for stronger sample/bit reduction.
p2 >> bass(P[0, 0, 7, 3], dur=1, sus=0.7, oct=3, amp=0.18,
           effect="SCLOrkDecimatorFX", fx_mix=0.7, fx_rate=5200,
           fx_bits=7, fx_drive=1.6, fx_tone=7000)

# Crossover distortion: raise fx_drive or fx_shape for a more gated edge.
p3 >> triangle(P[7, 10, 12, 10], dur=2, sus=1.5, oct=4, amp=0.12,
               effect="SCLOrkCrossoverDistortionFX", fx_mix=0.38,
               fx_drive=2.2, fx_shape=0.78, fx_tone=8500)

# Effects can also be ordered in a chain, subject to the four-insert limit:
# effects=[("SCLOrkPhazerFX", {"fx_mix": 0.3}),
#          ("SCLOrkDecimatorFX", {"fx_mix": 0.2})]
