# Example 231: patterned Sardine-style sleep offsets can spill into later cycles.
Clock.bpm = 100
Scale.default = Scale.minor
Root.default.set("C")

@swim(cycles=8, period=1)
def patterned_oversleep(p=1, i=0):
    # The delay pattern is selected by i: 0.5, 1.25, then repeats.
    p1 >> pluck([i % 7], dur=0.25, sus=0.22, oct=4, amp=0.16, pan=-0.2)
    sleep(P[0.5, 1.25])
    # On every other cycle this lands 0.25 beats into the next cycle.
    p2 >> triangle([(i + 3) % 7], dur=0.25, sus=0.22,
                   oct=3, amp=0.10, pan=0.2)
    again(patterned_oversleep, p=p, i=i + 1)
