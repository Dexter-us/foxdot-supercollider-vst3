# Example 142: FoxDot-style finite-pattern masks.
Clock.bpm = 96
Scale.default = Scale.major

source = P[0, 2, 4, 5, 7, 9]
mask = P[1, 0, 1, 0]

# select cycles the mask over the pattern/selector LCM (12 positions here).
p1 >> pluck(source.select(mask), dur=0.5, oct=4, amp=0.24)

# compress cycles its mask but never expands beyond the source's six items.
p2 >> triangle(source.compress(P[1, 0]), dur=0.5, oct=3, amp=0.15)

# The same finite mask helpers work on sample-symbol patterns.
p3 >> play(P["x", "-", "o", "-"].compress(P[1, 0]),
           dur=0.25, sample=0, amp=0.16)
