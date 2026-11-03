# Optional SpaceReverbs pack; Dexter U.S FoxDot v0.4.134 or newer.
# Both effects run at once, on separate players, and retain their tails
# between notes. These effect= / fx_* keywords are plugin extensions, not
# claims that stock FoxDot has JPverb/Greyhole player keywords.
Clock.bpm = 90
Scale.default = Scale.minor
Root.default.set("C")

# Lush JPverb on a sparse, pitched phrase. fx_t60 is in seconds.
p1 >> pluck(P[0, 2, 4, 7, 4, 2], dur=0.5, sus=0.35, oct=5, amp=0.22,
            effect="JPverb", fx_mix=0.35, fx_t60=4,
            fx_damp=0.4, fx_size=1.2, fx_earlydiff=0.75,
            fx_moddepth=0.08, fx_modfreq=1.5)

# Greyhole makes individual drum echoes easier to hear.
# fx_delay is seconds (not beats); fx_feedback sets the repeat amount.
p2 >> play("x---o---", dur=0.5, amp=0.18,
           effect="Greyhole", fx_mix=0.4, fx_delay=0.5,
           fx_damp=0.25, fx_size=1, fx_diff=0.6,
           fx_feedback=0.72, fx_moddepth=0.06)

# Set fx_mix=0 to hear the dry player, or 1 for effect-only output.
