# Example 147: concatenate, add, and take absolute pitches without mutation.
Clock.bpm = 100
Scale.default = Scale.minor

notes = P[-4, -2, 0, 3].concat(P[5, 7])
shifted = notes.add(2).abs()
all_positive = shifted.all(lambda value: value >= 0)
starts_on_two = shifted.startswith(2)
p1 >> pluck(shifted, dur=0.5, oct=4, amp=0.22)
