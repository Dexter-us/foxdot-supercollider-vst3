#!/usr/bin/env python3
"""Rebuild the optional CC0 user-synth demo SynthDefs with sclang."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SYNTHDEFS = ROOT / "spikes" / "vst3-callback" / "Portable" / "SynthDefs"
DEFINITIONS = {
    "CustomPluck": ("brightness", "sub"),
    "CustomTone": ("blend", "cutoff", "resonance"),
}


def compile_definition(name, directory):
    directory.mkdir(parents=True, exist_ok=True)
    source = SYNTHDEFS / f"{name}.scd"
    output = SYNTHDEFS / f"{name}.scsyndef"
    source_lines = source.read_text(encoding="utf-8").splitlines()
    expression = " ".join(
        line for line in source_lines if not line.lstrip().startswith("//")
    )
    expression = " ".join(expression.split())
    expression = expression.replace(
        "thisProcess.nowExecutingPath.dirname", json.dumps(SYNTHDEFS.as_posix())
    )
    env = dict(
        os.environ,
        XDG_DATA_HOME=str(directory / "data"),
        QT_QPA_PLATFORM="minimal",
        QT_QUICK_BACKEND="software",
        QT_OPENGL="software",
    )
    result = subprocess.run(
        ["sclang"],
        input=expression + "\n0.exit;\n",
        capture_output=True,
        text=True,
        env=env,
        timeout=120,
    )
    output_text = result.stdout + result.stderr
    if result.returncode or "ERROR:" in output_text:
        raise RuntimeError(
            f"sclang failed to compile {name}:\n{output_text[-6000:]}"
        )
    binary = output.read_bytes() if output.is_file() else b""
    required_names = (name,) + DEFINITIONS[name]
    if (
        not binary.startswith(b"SCgf")
        or len(binary) < 128
        or any(control.encode("ascii") not in binary for control in required_names)
    ):
        raise RuntimeError(f"{name}.scsyndef failed its SynthDef/name check")
    print(f"{name}: {len(binary)} bytes")


def main():
    if not shutil.which("sclang"):
        raise SystemExit(
            "sclang is needed only to rebuild the optional demo SynthDefs"
        )
    with tempfile.TemporaryDirectory(prefix="dexter-user-synthdefs-") as temp:
        directory = Path(temp)
        for name in DEFINITIONS:
            compile_definition(name, directory / name)


if __name__ == "__main__":
    main()
