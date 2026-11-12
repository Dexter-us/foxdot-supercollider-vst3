# Example 250: feedback repeats processed inside the delay loop.
Clock.bpm = 96

# Modes: 0 filtered, 1 saturated, 2 pitch-shifted,
# 3 stereo cross-feedback, 4 chance-gated every fx_interval seconds.
p1 >> pluck(P[0, 3, 7, 10], dur=0.5, sus=0.45, amp=0.2,
            effect="DexterFeedbackDelayFX", fx_mode=0, fx_time=0.28,
            fx_feedback=0.58, fx_pitch=1.25, fx_probability=1.0)

p2 >> sine(P[0, 7, 3], dur=1, sus=0.8, amp=0.16,
           effect="DexterFeedbackDelayFX", fx_mode=1, fx_time=0.22,
           fx_feedback=0.46, fx_pitch=1.25, fx_probability=1.0)

# Mode 2 shifts the recirculating repeats; dry notes stay at their original pitch.
p3 >> pluck(P[0, 4, 7], dur=1, sus=0.8, amp=0.14,
            effect="DexterFeedbackDelayFX", fx_mode=2, fx_time=0.3,
            fx_feedback=0.52, fx_pitch=1.5, fx_probability=1.0)

# Mode 4 makes a probability decision every 2.5 real-time seconds.
p4 >> play("x---", dur=1, sample=0, amp=0.12,
           effect="DexterFeedbackDelayFX", fx_mode=4, fx_time=0.2,
           fx_feedback=0.55, fx_pitch=1.25, fx_interval=2.5,
           fx_probability=0.65)
