# Example 101: slice positions can change by hit or with var()/linvar().
Clock.bpm = 92

# The four hits use four start/end pairs. Each pair stays within the WAV.
p1 >> play("x-o-", dur=0.5, sample=0,
          start=P[0.0, 0.15, 0.30, 0.45],
          end=P[0.28, 0.55, 0.72, 0.95],
          amp=0.28)

# The loop keeps its bounded sus while each new hit selects a different slice.
p2 >> play("o", dur=2, sus=1.8, sample=0,
          start=linvar([0.05, 0.35], [2, 2]),
          end=linvar([0.55, 0.95], [2, 2]),
          loop=True, amp=0.12, pan=0.25)
