# Example 177: per-note pitch shift plus a native per-player bitcrusher.
# pshift is in semitones (-24..24) and follows each note.
# bits=0 bypasses quantization; 1..24 lowers bit depth.
# crush=1 is full rate; larger values hold samples for a rougher rate reduction.
Clock.bpm = 100

p1 >> pluck(P[0, 4, 7, 9], dur=0.5, sus=0.45,
            pshift=var([0, 7, 12, -12], 2), amp=0.28)

# The bitcrusher sits on this player's summed output. Both controls are smoothed;
# changing them does not require a SynthDef or an external UGen.
p2 >> saw(P[0, 2, 4, 7], dur=0.25, sus=0.22,
          bits=var([0, 10, 6], 4), crush=var([1, 4, 12], 4),
          amp=0.18, pan=0.2)

# pshift also transposes WAV playback; reverse and very slow rates remain valid.
p3 >> play("x-o-", sample=0, dur=1, sus=0.85,
           pshift=P[0, 12, -12, 7], amp=0.14, pan=-0.25)

# A fixed setting is useful for comparing against the moving version.
p4 >> triangle(P[0, 7, 4, 2], dur=1, sus=0.85,
               pshift=P[0, 12, -12, 7], bits=8, crush=6,
               amp=0.12, pan=0.25)
