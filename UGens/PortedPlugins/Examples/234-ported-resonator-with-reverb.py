# Example 234: PortedPlugins Resonator and Fverb, plus reverb comparisons.
# Keep voices separated so each insert can be auditioned and adjusted.
Clock.bpm = 88
Scale.default = Scale.minor

# This is the new PortedPlugins Fverb stereo Dattorro reverb.
p1 >> pluck(P[0, 3, 7, 10], dur=1, oct=4, amp=0.08, pan=-0.45,
            effects=[("PortedFverbFX", {
                "fx_mix": 0.25, "fx_predelay": 1.5, "fx_decay": 46,
                "fx_damping": 6200, "fx_mod_depth": 0.35
            })])

# Ported Resonator plus Fverb, demonstrating the two new PortedPlugins UGens.
p2 >> pluck(P[0, 7, 3], dur=1.5, oct=4, amp=0.06, pan=0.4,
            effects=[
                ("PortedResonatorFX", {
                    "fx_mix": 0.22, "fx_freq": 220,
                    "fx_structure": 0.45, "fx_brightness": 0.35,
                    "fx_damping": 0.55
                }),
                ("PortedFverbFX", {
                    "fx_mix": 0.2, "fx_predelay": 0.8, "fx_decay": 38,
                    "fx_damping": 7400, "fx_mod_depth": 0.25
                })
            ])

# Existing optional-pack reverbs provide a quick A/B against Fverb.
p3 >> pluck(P[0, 7, 3], dur=2, oct=3, amp=0.05, pan=0.1,
            effect="JPverb", fx_mix=0.18, fx_t60=2.8, fx_damp=0.3,
            fx_size=1.1, fx_earlydiff=0.7, fx_moddepth=0.1, fx_modfreq=2.0)
