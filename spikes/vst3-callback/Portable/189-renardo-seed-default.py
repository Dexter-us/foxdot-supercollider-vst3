# Example 189: Renardo's Seed.default controls newly created random patterns.
# Re-running the script with the same default produces the same patterns.
Clock.clear()
Clock.bpm = 108
Scale.default = Scale.minor
Seed.default = 23

p1 >> pluck(PRand([0, 2, 3, 5, 7], length=16), dur=0.25,
           oct=4, amp=0.2)
p2 >> triangle(PWhite(0, 7, length=16), dur=0.25,
               oct=3, amp=0.12, pan=0.25)

# An explicit seed overrides Seed.default for just this generator.
p3 >> sine(PRand(0, 12, length=16, seed=99), dur=0.25,
           oct=5, amp=0.08, pan=-0.25)

# Set Seed.default = None to restore normal unseeded randomness for generators
# created afterward.
