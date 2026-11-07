# Example 154: appending mutates shared references; copy() stays independent.
Clock.bpm = 96
Scale.default = Scale.minor

phrase = P[0, 2, 4]
shared_phrase = phrase
phrase.append(7)
phrase.extend(P[9, 11])
independent_phrase = phrase.copy()
phrase.append(0)

p1 >> triangle(phrase, dur=0.5, oct=4, amp=0.18)
p2 >> pluck(shared_phrase, dur=0.5, oct=3, amp=0.14, pan=0.25)
p3 >> saw(independent_phrase, dur=0.5, oct=2, amp=0.12, pan=-0.25)
