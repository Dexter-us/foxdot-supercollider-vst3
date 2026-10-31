# Example 77: finite P patterns can sequence sample symbols and transform them.
# reverse() and rotate() follow FoxDot's finite Pattern behavior.
Clock.bpm = 100

# P patterns accept one sample symbol or a space-rest per item.
p1 >> play(P["x", "o", "x", " "].reverse(), dur=0.5,
           sample=P[0, 1], amp=0.30)

p2 >> play(P["a", "o", "a", " "].rotate(1), dur=1,
           sample=0, pan=0.25, amp=0.22)
