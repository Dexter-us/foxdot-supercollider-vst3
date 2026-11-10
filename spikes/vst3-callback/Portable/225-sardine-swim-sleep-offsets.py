# Example 225: bounded beat-accurate sleep offsets inside @swim.
# sleep() advances later events in this cycle; it does not pause Python.
Clock.bpm = 108
Scale.default = Scale.minor
Root.default.set("C")

@swim(cycles=16, period=1)
def call_and_response(p=1, i=0):
    p1 >> pluck([i % 7], dur=0.25, sus=0.22, oct=4, amp=0.19, pan=-0.2)
    sleep(0.5)
    p2 >> triangle([(i % 7) + 3], dur=0.25, sus=0.22,
                   oct=4, amp=0.12, pan=0.2)
    again(call_and_response, p=p, i=i + 1)
