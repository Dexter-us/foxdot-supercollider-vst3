# Example 220: bounded @swim branches add a fill every fourth beat.
# The 16 cycles are expanded into one finite arrangement; no live Python
# loop remains running after the pattern is compiled.
Clock.bpm = 108
Scale.default = Scale.minor

@swim(cycles=16, period=1)
def groove(p=1, i=0):
    if i % 4 == 3:
        p1 >> saw(P[0, 2, 4, 7].rotate(i % 4), dur=0.25,
                  sus=0.22, oct=4, amp=0.16, pan=-0.2)
    else:
        p1 >> saw(P[0, 0, 2, 0].rotate(i % 4), dur=0.25,
                  sus=0.22, oct=4, amp=0.16, pan=-0.2)
    p2 >> play("x---", dur=0.25, sample=0, amp=0.18, pan=0.2)
    again(groove, p=p, i=i + 1)
