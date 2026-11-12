# Example 188: Renardo's Clock.clear() syntax, implemented by stopping all
# eight embedded VST players before starting this script's own parts.
Clock.clear()
Clock.bpm = 112
Scale.default = Scale.minor

p1 >> play("x-o-", dur=0.5, amp=0.45)
p2 >> pluck(P[0, 3, 7, 10], dur=0.5, sus=0.42, oct=4, amp=0.2)
p3 >> triangle(P[0, 7, 3, 10], dur=1, sus=0.85, oct=3, amp=0.12)

# In the embedded engine, Clock.clear() is the safe stop-all/reset command.
# It does not create a SuperCollider server or clear the host's DAW transport.
