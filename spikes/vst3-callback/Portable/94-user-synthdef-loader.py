# Example 94: play a precompiled SynthDef from Portable/SynthDefs.
# DexterPluck.scsyndef is included in Portable/SynthDefs. To edit the patch,
# compile its .scd source offline with sclang and restart the DAW.
tempo(112)
Scale.default.set("minor")

p1 >> userSynth("DexterPluck", P[0, 2, 4, 7, 4, 2], dur=0.5, sus=0.42,
               oct=4, amp=0.32, pan=-0.18, atk=0.008, rel=0.16)

p2 >> userSynth("DexterPluck", P[7, 4, 2, 0], dur=1, sus=0.85,
               oct=5, amp=0.18, pan=0.22, atk=0.015, rel=0.24)
