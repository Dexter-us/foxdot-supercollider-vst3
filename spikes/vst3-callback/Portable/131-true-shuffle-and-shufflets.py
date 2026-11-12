# Example 131: true_shuffle scrambles expanded notes; shufflets makes strummed groups.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("C")

phrase = P[0, 2, 4, 7, 9, 7, 4, 2]
scrambled = phrase.true_shuffle(2)
chordPasses = P[0, 2, 4, 7].shufflets(4)

p1 >> pluck(scrambled, dur=0.5, oct=4, amp=0.18)
p2 >> triangle(chordPasses, dur=2, sus=1.8, oct=3,
               strum=0.12, amp=0.12, pan=0.25)
p3 >> play("x---", dur=1, sample=0, amp=0.14)
