# Example 223: Sardine-style @swim cycles with a bounded period pattern.
# Periods for successive cycles are 0.5, 1, 0.25, 0.75 beats, then repeat.
Clock.bpm = 108
Scale.default = Scale.minor
Root.default.set("C")

@swim(cycles=8, periods=P[0.5, 1.0, 0.25, 0.75])
def changing_cycle(p=0.5, i=0):
    if i % 4 == 0:
        p1 >> pluck(P[0, 2], dur=0.25, sus=0.22, oct=4, amp=0.2)
    elif i % 4 == 1:
        p1 >> pluck(P[2, 4], dur=0.25, sus=0.22, oct=4, amp=0.2)
    elif i % 4 == 2:
        p1 >> pluck(P[4, 7], dur=0.25, sus=0.22, oct=4, amp=0.2)
    else:
        p1 >> pluck(P[7, 5], dur=0.25, sus=0.22, oct=4, amp=0.2)

    # The decorator selects the next cycle period; dur still spaces the notes.
    again(changing_cycle, i=i + 1)
