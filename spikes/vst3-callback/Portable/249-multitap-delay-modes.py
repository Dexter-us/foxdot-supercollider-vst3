# Example 249: tempo-synced, free-time, and probabilistic multi-tap delay.
Clock.bpm = 112

# fx_mode=0 uses beat subdivisions and follows fx_bpm.
p1 >> pluck(P[0, 3, 7], dur=1, sus=0.8, amp=0.22,
            effect="DexterMultiTapDelayFX", fx_mode=0, fx_time=0.25,
            fx_taps=4, fx_feedback=0.42, fx_probability=0.75,
            fx_bpm=Clock.bpm)

# fx_mode=1 treats fx_time as seconds and always enables the selected taps.
p2 >> play("x---", dur=1, sample=0, amp=0.18,
           effect="DexterMultiTapDelayFX", fx_mode=1, fx_time=0.18,
           fx_taps=3, fx_feedback=0.32, fx_probability=1.0,
           fx_bpm=Clock.bpm)

# fx_mode=2 makes a fresh per-tap probability decision on each detected hit.
p3 >> play("o---", dur=1, sample=0, amp=0.15,
           effect="DexterMultiTapDelayFX", fx_mode=2, fx_time=0.25,
           fx_taps=4, fx_feedback=0.38, fx_probability=0.45,
           fx_bpm=Clock.bpm)
