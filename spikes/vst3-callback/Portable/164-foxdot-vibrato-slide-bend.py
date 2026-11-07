# Example 164: FoxDot vibrato and per-note slide/bend controls.
# vib is the vibrato rate; vibdepth (or the older depth name) is its depth.
# FoxDot's slide/slidefrom/bend SynthDefs apply amplitude envelopes, not pitch
# glides. Each is kept on a separate player here so the contour is easy to hear.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("C")

# Ramps the note level from 1 to 1 + slide during the part after slidedelay.
p1 >> saw([0, 3, 5, 7], dur=1, sus=0.9, slide=P[0.0, 0.55, -0.25, 0.0],
          slidedelay=var([0.35, 0.5], [4, 4]), amp=0.18)

# Starts at 1 + slidefrom, holds for slidedelay, then returns to 1.
p2 >> triangle([7, 5, 3, 0], dur=1, sus=0.9,
               slidefrom=P[0.35, 0.0, -0.2, 0.0],
               slidedelay=0.4, amp=0.15, pan=-0.25)

# Bend rises to its peak after benddelay, then returns to unity by note end.
p3 >> viola([0, 5, 3, 7], dur=1, sus=0.9,
            bend=P[0.25, -0.15, 0.35, 0.0], benddelay=0.2,
            vib=5, vibdepth=var([0.02, 0.04], [4, 4]),
            amp=0.14, pan=0.25)
