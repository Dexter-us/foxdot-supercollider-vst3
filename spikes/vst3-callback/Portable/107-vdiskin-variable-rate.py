# Example 107 — native VDiskIn, optional DiskIO pack.
# rate=1 is natural file speed, even when the host and file rates differ.
# Change the scalar rate and press Run: the stream is re-cued from the start.
# This first disk milestone does not claim var()/linvar() stream modulation.
DiskIn.stop(slot=1)
VDiskIn.play("DiskIO/drum-loop.wav", slot=1, rate=0.75,
            loop=True, amp=0.55, pan=0.0)

# Try rate=1.0, rate=1.5, or rate=0.5 above.
# Forward-only VDiskIn is not reverse playback or pitch-preserving stretch.
# VDiskIn.stop(slot=1)
