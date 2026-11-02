# Example 122: Native FoxDot-style pattern inversion plus the existing transpose helper.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("C")

# invert() reflects numeric notes around the phrase's highest note.
lead = P[0, 2, 4, 7, None, 4]
p1 >> pluck(lead.invert(), dur=0.5, oct=4, amp=0.24)

# The plugin's transpose(pattern, interval) helper transposes notes and chords.
chords = P[PGroup(0, 4, 7), None, PGroup(2, 5, 9), PGroup(0, 3, 7)]
p2 >> triangle(transpose(chords, 2), dur=1, oct=3, amp=0.12, pan=0.3)

p3 >> play("x---x---", dur=0.5, amp=0.11)
