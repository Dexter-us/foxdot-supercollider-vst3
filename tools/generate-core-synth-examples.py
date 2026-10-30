#!/usr/bin/env python3
"""Generate one isolated, editable Portable test for every new core synth."""
from pathlib import Path
from importlib.util import module_from_spec, spec_from_file_location

spec = spec_from_file_location("bank", Path(__file__).with_name("build-foxdot-synth-bank.py"))
bank = module_from_spec(spec)
spec.loader.exec_module(bank)
portable = bank.SOURCE.parent / "Portable"
for number, name in enumerate(bank.NAMES, 43):
    octave = 5
    extras = ""
    if name in ("sawbass", "dbass", "dab"):
        octave = 5  # These original patches already transpose down internally.
    if name in ("klank", "feel", "glass", "soft", "space"):
        octave = 4
    if name == "nylon":
        extras = ", rate=1"
    elif name == "snick":
        extras = ", rate=2"
    elif name == "ambi":
        extras = ", rate=0.5"
    elif name == "quin":
        extras = ", rate=0.2"
    elif name == "swell":
        extras = ", rate=1"
    if name == "viola":
        extras = ", vib=6"
    amplitude = 0.5 if name not in ("scratch", "quin") else 0.2
    text = f"""# Example {number}: original FoxDot {name} graph (core UGens only).
# Test this synth alone first. Longer spacing lets its envelope finish.
# Change notes/rate/amp, then Run again to test live edits and retriggering.
Clock.bpm = 90
Scale.default = Scale.minor
Root.default.set("C")

# Make this a solo test even after a multi-player example.
p2.stop()
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

p1 >> {name}(P[0, 2, 4, 7], dur=4, sus=0.5, oct={octave},
             amp={amplitude}, reverb="none"{extras})

# To stop: uncomment this line without adding leading spaces:
# p1.stop()
"""
    (portable / f"{number:02d}-foxdot-{name}.py").write_text(text)
print("Generated Portable examples 43–68")
