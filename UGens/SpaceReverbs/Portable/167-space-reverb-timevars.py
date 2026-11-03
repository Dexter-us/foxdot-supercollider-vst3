# Optional SpaceReverbs pack; v0.4.134 or newer.
# var switches at beat boundaries; linvar continuously changes the controls.
# The engine smooths normalized control updates before writing SC controls.
Clock.bpm = 100
Scale.default = Scale.minor

p1 >> pluck(P[0, 4, 7, 9], dur=1, sus=0.3, amp=0.2,
            effect="JPverb", fx_mix=linvar([0.15, 0.5], 8),
            fx_t60=var([2, 5], 8), fx_damp=0.4, fx_size=1.1,
            fx_earlydiff=0.75, fx_moddepth=0.05, fx_modfreq=2)

p2 >> play("x---o---", dur=0.5, amp=0.17,
           effect="Greyhole", fx_mix=linvar([0.2, 0.45], 8),
           fx_delay=0.5, fx_damp=0.3, fx_size=1,
           fx_diff=0.7, fx_feedback=var([0.5, 0.78], 8),
           fx_moddepth=0.05)
