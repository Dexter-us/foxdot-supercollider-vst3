# Example 162: each play-string letter reads its own Portable WAV bank.
# Add alternative licensed WAVs to x/lower, then switch sample=0 to sample=1.
# The supplied x bank has two test kicks; the o bank has one test snare.
Clock.bpm = 100

p1 >> play("x---x---", dur=0.5, sample=0, amp=0.38)
p2 >> play("o---", dur=1, sample=0, amp=0.30, pan=0.2)

# The second x-bank file is selected for every hit here.
p3 >> play("x---x---", dur=0.5, sample=1, amp=0.20, pan=-0.2)

# |x1| overrides the bank selection for only the marked hits.
p4 >> play("|x1|---x---", dur=0.5, sample=0, amp=0.16, pan=0.35)
