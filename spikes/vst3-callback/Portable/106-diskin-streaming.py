# Example 106 — optional DiskIO pack, VST 0.4.80 or newer.
# Install its Portable/SCPlugins and Portable/Samples/DiskIO folders, then
# fully restart the DAW. No external SuperCollider server or sclang is used.
#
# DiskIn follows native SC behavior: it consumes one file frame per host frame.
# At a host rate other than the demo WAV's 48 kHz, speed/pitch changes slightly.
# Example 107 uses VDiskIn for automatic source/host sample-rate compensation.
DiskIn.play("DiskIO/drum-loop.wav", slot=1, loop=True, amp=0.55)

# Run this line on its own to release the file and stream buffer:
# DiskIn.stop(slot=1)
# The plugin's Stop button also stops streams and finalizes any recording.
