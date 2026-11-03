#!/usr/bin/env python3
"""Check checked-in Chorus/Flanger SynthDef ABI and stock-UGen dependencies."""
from pathlib import Path
import struct
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: test-modulation-synthdefs.py plugin-directory")
plugin = Path(sys.argv[1]).resolve()
defs = plugin / "Portable/SynthDefs"
expected = {
    "Chorus": ("mix rate depth delay spread feedback tone",
               {"DelayC", "In", "LeakDC", "LocalIn", "LocalOut",
                "LPF", "Out", "SinOsc", "XFade2"}),
    "Flanger": ("mix rate depth delay spread feedback tone",
                {"DelayC", "In", "LeakDC", "LocalIn", "LocalOut",
                 "LPF", "Out", "SinOsc", "XFade2"}),
}

def inspect(path):
    data = path.read_bytes()
    offset = 4
    assert data[:4] == b"SCgf", path
    def read(fmt):
        nonlocal offset
        value = struct.unpack_from(fmt, data, offset)[0]
        offset += struct.calcsize(fmt)
        return value
    def string():
        nonlocal offset
        length = read(">B")
        result = data[offset:offset + length].decode("utf-8")
        offset += length
        return result
    assert read(">i") == 2 and read(">H") == 1
    name = string()
    constants = read(">i")
    offset += constants * 4
    defaults = [read(">f") for _ in range(read(">i"))]
    controls = {}
    for _ in range(read(">i")):
        key = string()
        controls[key] = read(">i")
    units = set()
    for _ in range(read(">i")):
        units.add(string())
        read(">B")  # calculation rate
        inputs, outputs = read(">i"), read(">i")
        read(">h")  # special index
        offset += inputs * 8 + outputs
    assert read(">H") == 0 and offset == len(data)
    return name, controls, defaults, units

for name, (control_names, units_required) in expected.items():
    path = defs / (name + ".scsyndef")
    synth_name, controls, defaults, units = inspect(path)
    names = ("out inbus " + control_names).split()
    assert synth_name == name
    assert controls == {control: index for index, control in enumerate(names)}
    assert len(defaults) == 9, (name, len(defaults))
    assert units_required <= units, (name, sorted(units_required - units))
    assert name + "Raw" not in units
    source = (defs / (name + ".scd")).read_text()
    for unit in units_required:
        assert unit in source
    print("PASS", name, "9-control stereo SynthDef; stock UGens only")
