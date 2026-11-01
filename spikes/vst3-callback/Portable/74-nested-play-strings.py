# Example 74: FoxDot parentheses lace a group across repetitions; square
# brackets subdivide their contents into one step's time span.
Clock.bpm = 108

# The two short hits fit between the x and o on each line.
p1 >> play("x[--]o", dur=1, amp=0.34, pan=-0.15)

# (a ) plays a then a rest on alternating pattern cycles.
p2 >> play("(a )-", dur=0.5, sample=var([0, 1], 4),
           rate=var([1, -1], 8), amp=0.22, pan=0.2)
