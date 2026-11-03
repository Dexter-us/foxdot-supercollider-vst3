# Example 150: copy a phrase before making an independent variation.
Clock.bpm = 96
Scale.default = Scale.minor

source_phrase = P[0, 2, 4, 7]
edited_phrase = source_phrase.copy()
edited_phrase[0] = 5
edited_phrase[1] = edited_phrase[1] + 1

p1 >> pluck(source_phrase, dur=0.5, oct=4, amp=0.18)
p2 >> triangle(edited_phrase, dur=0.5, oct=4, amp=0.12, pan=0.25)
