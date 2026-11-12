# Example 248: test the editor's stereo mic-loop controls.
#
# 1. Route your host's audio input to the VST's stereo input.
# 2. Run this quiet reference pattern, then click "Record mic loop".
# 3. Play or sing for up to 8 seconds; click "Finish loop".
# 4. Click "Play loop" to hear the captured loop, or "Stop loop" to stop it.
# Recording is explicit, in-memory, and not monitored live or exported to WAV.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> pluck(P[0, 3, 5, 7], dur=1, oct=4, amp=0.14, pan=-0.25)
p2 >> sine(P[0, 7], dur=2, oct=3, amp=0.08, pan=0.25)
