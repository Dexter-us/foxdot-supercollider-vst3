# Example 183: bounded FoxDot-style index and random-choice generators.
Clock.bpm = 100
Scale.default = Scale.minor
Root.default.set("C")

# PIndex walks through 0..11 once, then the player cycles the finite phrase.
p1 >> pluck(PIndex(12), dur=0.25, amp=0.16)

# PxRand is seeded and never repeats the previous choice.
p2 >> saw(PxRand(P[0, 2, 4, 7, 9], length=32, seed=23),
          dur=0.5, amp=0.14)

# PwRand repeats values in proportion to their integer weights.
p3 >> triangle(PwRand(P[0, 3, 5, 7, 10], P[1, 3, 2, 2, 1],
                     length=32, seed=31),
               dur=0.5, amp=0.15, pan=0.2)

# PxRand can choose sample symbols too; adjacent hits use different symbols.
p4 >> play(PxRand("x-o-", length=32, seed=44),
           sample=0, dur=0.25, amp=0.12)
