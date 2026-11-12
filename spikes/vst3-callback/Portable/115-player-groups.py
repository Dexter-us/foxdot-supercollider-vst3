# Example 115: FoxDot-style groups share live controls and actions.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("A")

p1 >> bass([0, 2, 4, 3], dur=0.5, sus=0.42, amp=0.55, oct=5)
p2 >> pluck([7, 5, 3, 2], dur=1, sus=0.8, amp=0.42, oct=4, pan=0.25)

# One assignment updates both players; var/linvar control modulation is retained.
low_end = Group(p1, p2)
low_end.amp = 0.22
low_end.cutoff = linvar([800, 2200], [4, 4])
low_end.reverb = "freeverb"
low_end.mix = 0.18

# This bounded schedule is applied independently to each group member.
low_end.every(2, "reverse", cycle=8, ident="shared-phrase")

p3 >> play("x---", dur=1, sample=0, amp=0.4)

# Try these separately while performing:
# low_end.solo()  # mute players outside the group
# low_end.unmute()
# low_end.only()  # stop players outside the group
# low_end.stop()  # stop both members
# Master().amp = 0.3  # shared gain for currently active players
