# A small FoxDot-style script running directly inside the VST3.
# This embedded compatibility subset schedules notes locally; it does not start
# FoxDot's external clock/server or send OSC. Sample-string players are not included yet.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> saw([0, 2, 4, 7], dur=[0.5, 0.5, 1, 1], oct=5, sus=0.4, amp=0.7, pan=-0.35)
p2 >> pulse([0, None, 3, None], dur=0.5, oct=4, sus=0.2, amp=0.25, pan=0.35)
