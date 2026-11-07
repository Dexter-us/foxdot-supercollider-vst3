# Example 181: bounded FoxDot-style pattern generators.
# Random helpers accept a seed here so you can replay the same sequence.
p1 >> pluck(P10(16, seed=17) * P[0, 7], dur=0.25, amp=0.22)
p2 >> sine(PSine(16) * 12, dur=0.5, amp=0.14)
p3 >> triangle(PTri(0, 8, 2), dur=0.5, amp=0.18)
p4 >> blip(PBern(size=16, ratio=0.35, seed=31) * P[0, 3, 5, 7],
           dur=0.25, amp=0.12)
p5 >> bass(PSum(8, 4), dur=0.5, amp=0.18)
