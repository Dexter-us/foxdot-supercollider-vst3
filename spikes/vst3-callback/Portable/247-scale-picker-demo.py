# Example 247: choose a scale from the editor's Scale.default context menu.
# Right-click the Scale.default line in the editor to open the scale picker.
Clock.bpm = 96
Scale.default = Scale.minor

# Try changing the highlighted line with the right-click menu:
# major, dorian, mixolydian, pentatonic, and other built-in scales.
p1 >> pluck(P[0, 2, 4, 7, 4, 2], dur=0.5, oct=4, amp=0.22)
p2 >> sine(P[0, 4, 7], dur=2, oct=3, amp=0.10, pan=-0.25)
