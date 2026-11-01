# Example 76: FoxDot's |x1| form selects one sample variant for that hit.
# The digit is a single sample index; other hits still use sample=.
Clock.bpm = 100

# x1 is the alternate tight kick; a1 is the alternate chirp fixture.
p1 >> play("|x1|x|x0|o|a1|a", dur=0.5, sample=0, amp=0.32)
