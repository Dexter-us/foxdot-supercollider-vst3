# Example 195: Limut-inspired ping-pong delay and rhythmic ducking.
# Install the optional LimutFX UGen pack first; timing controls use seconds.
Clock.bpm = 120
Scale.default = Scale.minor

# At 120 BPM, 0.5 seconds is one beat. Delay time glides smoothly.
p1 >> pluck(P[0, 3, 7, 10], dur=0.5, oct=4, amp=0.20,
            effect="LimutPingPong", fx_mix=0.32,
            fx_time=var([0.25, 0.375], 4), fx_feedback=0.52)

# The duck envelope drops at each period and recovers over attack seconds.
p2 >> bass(P[0, 0, 7, 3], dur=0.5, oct=5, amp=0.18,
           effect="LimutDuck", fx_mix=1,
           fx_period=0.5, fx_depth=0.68, fx_length=0.07, fx_attack=0.18)

# Both inserts can be ordered on one player; the second insert hears the first.
p3 >> triangle(P[7, 10, 12, 10], dur=1, oct=4, amp=0.10,
               effects=[
                   ("LimutPingPong", {
                       "fx_mix": 0.24, "fx_time": 0.32, "fx_feedback": 0.42}),
                   ("LimutDuck", {
                       "fx_mix": 0.55, "fx_period": 0.5, "fx_depth": 0.55,
                       "fx_length": 0.06, "fx_attack": 0.20})
               ])
