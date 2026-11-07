# Example 219: a bounded Sardine-style @swim phrase.
# The editor expands eight cycles into one finite 16-beat arrangement;
# it does not keep a Python loop running in the audio thread.
Clock.bpm = 100
Scale.default = Scale.minor

@swim(cycles=8, period=2)
def groove(p=2, i=0):
    # The sample choice and the rotated bass phrase change each cycle.
    p1 >> play("x-o-", dur=0.5, sample=i % 2, amp=0.16, pan=-0.2)
    p2 >> bass(P[0, 2, 4, 7].rotate(i % 4), dur=0.5, sus=0.42,
               oct=3, amp=0.14, pan=0.2)
    again(groove, p=p, i=i + 1)
