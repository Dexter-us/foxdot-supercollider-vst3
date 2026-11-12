# Example 118: Group.root transposes every numeric player in the group.
Clock.bpm = 96
Scale.default.set("major")
Root.default.set("C")

p1 >> pluck(P[0, 2, 4, 6], dur=1, sus=0.65, oct=5,
            amp=0.2, reverb="freeverb", room=0.2, mix=0.12)
p2 >> triangle(P[4, 2, 0, 2], dur=2, sus=1.4, oct=4,
               amp=0.14, pan=0.25)

melody = Group(p1, p2)
melody.root = "D"  # Note name: both players now use D major.

# A separate player changes root on note onsets: MIDI G, then MIDI C.
p3 >> sine(P[0, 2, 4, 6], dur=1, sus=0.7, oct=5,
           amp=0.1, pan=-0.25)
answer = Group(p3)
answer.root = var([67, 72], [2, 2])
