# Example 139: PRhythm accents inside Pvar phrase switching.
Clock.bpm = 84
Scale.default = Scale.minor

# Each three-beat phrase has one single hit followed by a two-beat grouped
# rhythm. The grouped offsets repeat that phrase note without breaking Pvar.
p1 >> pluck(Pvar([P[0, 2], P[4, 6]], [3, 3]),
            dur=PRhythm([1, (3, 8)]), sus=0.28, oct=4, amp=0.28)

# A sample-symbol phrase can use the same grouped duration pattern.
p2 >> play(Pvar(["x-", "o-"], [3, 3]),
           dur=PRhythm([1, (3, 8)]), sample=0, sus=0.2, amp=0.14, pan=0.2)
