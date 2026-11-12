# Example 222: make a bounded @swim phrase evolve every few cycles.
# every(4) is true on cycle indices 0, 4, 8, 12; offset selects another phase.
Clock.bpm = 108
Scale.default = Scale.minor

@swim(cycles=16, period=1)
def changing_phrase(p=1, i=0):
    if every(4, offset=3):
        p1 >> pluck(P[7, 9, 11, 14], dur=0.25, sus=0.22,
                    oct=4, amp=0.2, pan=-0.15)
    else:
        p1 >> pluck(P[0, 2, 4, 7], dur=0.25, sus=0.22,
                    oct=4, amp=0.2, pan=-0.15)

    again(changing_phrase, p=p, i=i + 1)
