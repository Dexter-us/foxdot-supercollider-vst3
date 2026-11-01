# Example 108 — record the whole post-master stereo VST output with DiskOut.
# This explicitly creates a NEW float32 WAV in Portable/Recordings.
# It never overwrites a file. The path is shown in the editor status.
# Recording ends automatically at 8 seconds and the WAV header is finalized.
Clock.bpm = 100
p1 >> sine(P[0, 2, 4, 7, 4, 2], dur=0.5, sus=0.35,
           oct=4, amp=0.22, atk=0.02, rel=0.12)
DiskOut.record(seconds=8)

# Run separately for an early stop and finalization:
# DiskOut.stop()
# The VST Stop button also finalizes. Do not remove files while recording.
