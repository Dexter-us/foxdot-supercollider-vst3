# Dexter extension: MouseX and MouseY are SuperCollider UI UGens, not standard FoxDot player syntax.
# Move the cursor horizontally to pan p1; move it vertically to sweep p2's filter cutoff.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> triangle(P[0, 2, 4, 7], dur=0.5, oct=5, sus=0.35, amp=0.3, pan=MouseX())
p2 >> saw(P[0, None, 3, None], dur=0.5, oct=3, sus=0.25, amp=0.18, cutoff=MouseY(), pan=0.3)
