# Example 221: isolated test of the precompiled BlitB3Saw UGen.
# Requires AntiAliasingOscillators.scx in Portable/SCPlugins and
# BlitB3SawProbe.scsyndef in Portable/SynthDefs. Restart the DAW after install.
Clock.bpm = 96
Scale.default = Scale.major

p1 >> userSynth("BlitB3SawProbe", P[0, 2, 4, 7], dur=0.5, oct=4, amp=0.16)
# Adjust the BlitB3Saw leak slider in the Players panel to compare timbre.
