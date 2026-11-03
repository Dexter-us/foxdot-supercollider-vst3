# Example 168: Chorus adds a moving, widened double to the dry notes.
# effect= and fx_* are Dexter U.S. FoxDot VST extensions.
Clock.bpm = 96
Scale.default = Scale.major
Root.default.set("A")

p1 >> pluck(P[0, 2, 4, 7], dur=1, oct=4, sus=0.8, amp=0.18,
            effect="Chorus", fx_mix=0.35, fx_rate=0.8,
            fx_depth=0.45, fx_delay=0.018, fx_spread=0.9,
            fx_feedback=0.08, fx_tone=11000)
