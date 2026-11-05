#!/usr/bin/env python3
"""Validate the three GPL SCLOrk-derived stereo insert SynthDefs."""
from pathlib import Path
import struct
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: test-sclork-synthdefs.py plugin-directory")

plugin = Path(sys.argv[1]).resolve()
defs = plugin / "Portable" / "SynthDefs"
expected = {
    "SCLOrkPhazerFX": (
        "mix rate depth center spread rq tone",
        {"BAllPass", "In", "Lag", "LPF", "Out", "SinOsc", "XFade2"}),
    "SCLOrkDecimatorFX": (
        "mix rate bits drive tone",
        {"Clip", "In", "Lag", "LFPulse", "Latch", "LPF", "Out", "XFade2"}),
    "SCLOrkCrossoverDistortionFX": (
        "mix drive nonlinearity tone",
        {"Clip", "In", "Lag", "LPF", "Out", "XFade2"}),
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


for name, (effect_controls, required_units) in expected.items():
    compiled = defs / (name + ".scsyndef")
    source = defs / (name + ".scd")
    assert compiled.is_file() and source.is_file(), name
    synth_name, controls, defaults, units = inspect(compiled)
    control_names = ("out inbus " + effect_controls).split()
    assert synth_name == name, (name, synth_name)
    assert controls == {key: index for index, key in enumerate(control_names)}, (
        name, controls)
    assert len(defaults) == len(control_names), (name, len(defaults))
    assert required_units <= units, (name, sorted(required_units - units))
    assert not any(unit.endswith("Raw") for unit in units), (name, units)
    text = source.read_text(encoding="utf-8")
    assert "In.ar(inbus, 2)" in text and "Out.ar(out" in text
    assert "writeDefFile" in text and "GPL-3.0-only" in text
    print("PASS", name, "stereo ABI", len(control_names),
          "stock-UGen definition")

attribution = defs / "SCLOrkSynths-ATTRIBUTION.txt"
license_path = plugin / "Licenses" / "SCLOrkSynths-GPL-3.0.txt"
assert attribution.is_file() and license_path.is_file()
assert license_path.stat().st_size > 30000
