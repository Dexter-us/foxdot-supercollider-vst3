# Example 198: a short gated-reverb burst with live FoxDot time variation.
Clock.bpm = 108
Scale.default = Scale.minor

p1 >> pluck([0, 7, 3, 10], dur=1, sus=0.28, oct=4, amp=0.24,
            effect="LimutGatedVerb", fx_mix=0.85, fx_room=0.82,
            fx_damp=0.45, fx_threshold=0.018,
            fx_hold=var([0.12, 0.28], 4), fx_attack=0.01,
            fx_release=0.16)

# This insert chain uses the pitch shifter before the shimmer reverb.
p2 >> pluck([0, 3, 7, 10], dur=2, sus=1.6, oct=3, amp=0.12,
            effects=[
                ("LimutShifter", {
                    "fx_mix": 0.7, "fx_ratio": 1.5, "fx_window": 0.12}),
                ("LimutShimmer", {
                    "fx_mix": 0.42, "fx_ratio": 2.0, "fx_window": 0.12,
                    "fx_room": 0.75, "fx_damp": 0.4}),
            ])

p3 >> pluck([0, 3, 7, 10], dur=2, sus=1.6, oct=3, amp=0.08)
