# Example 83: pulse-string and Euclidean rhythm helpers control sample timing.
Clock.bpm = 96

# PBeat turns the spaces between x pulses into step durations.
p1 >> play(P["x", " ", "o", " "], dur=PBeat("x xxx"), sample=0, amp=0.28)

# PDur(5, 8) places five evenly distributed hits across eight steps.
p2 >> play(P["-", "-", "-", "-", "-"], dur=PDur(5, 8, dur=0.25),
          sample=0, amp=0.10)
