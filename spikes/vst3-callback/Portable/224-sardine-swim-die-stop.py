# Example 224: a finite @swim phrase with a same-name @die stop command.
# Run this file to start the bounded phrase. To stop it before it finishes,
# run the separate snippet at the end of the file.
Clock.bpm = 108
Scale.default = Scale.minor
Root.default.set("C")

@swim(cycles=16, periods=P[0.5, 1.0, 0.25, 0.75])
def stoppable_phrase(p=0.5, i=0):
    if i % 4 == 0:
        p1 >> pluck(P[0, 2], dur=0.25, sus=0.22, oct=4, amp=0.2)
    elif i % 4 == 1:
        p1 >> pluck(P[2, 4], dur=0.25, sus=0.22, oct=4, amp=0.2)
    elif i % 4 == 2:
        p1 >> pluck(P[4, 7], dur=0.25, sus=0.22, oct=4, amp=0.2)
    else:
        p1 >> pluck(P[7, 5], dur=0.25, sus=0.22, oct=4, amp=0.2)

    again(stoppable_phrase, i=i + 1)

# To stop the named bounded phrase while it is playing, run this separately:
# @die
# def stoppable_phrase():
#     pass
