#!/usr/bin/env python3
"""Compile the two stock-UGen chorus/flanger insert definitions with sclang."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "spikes/vst3-callback/Portable/SynthDefs"
NAMES = ("Chorus", "Flanger")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sclang", default=shutil.which("sclang"))
    args = parser.parse_args()
    if not args.sclang:
        raise SystemExit("sclang is required only to regenerate these checked-in SynthDefs")
    with tempfile.TemporaryDirectory(prefix="dexter-modulation-synthdefs-") as folder:
        folder = Path(folder)
        script = folder / "build.scd"
        commands = []
        for name in NAMES:
            source = (DESTINATION / (name + ".scd")).read_text()
            marker = ".writeDefFile;"
            if not source.rstrip().endswith(marker):
                raise ValueError("Unexpected SynthDef source format: " + name)
            source = source.rstrip()[:-len(marker)] + \
                ".writeDefFile(\"" + DESTINATION.as_posix() + "\");"
            commands.append(source)
        script.write_text("\n".join(commands) + "\n0.exit;\n")
        env = dict(os.environ, QT_QPA_PLATFORM="minimal",
                   QT_QUICK_BACKEND="software", QT_OPENGL="software",
                   QTWEBENGINE_DISABLE_SANDBOX="1")
        result = subprocess.run([args.sclang, str(script)], env=env,
                                capture_output=True, text=True, timeout=60)
        if result.returncode or "ERROR:" in result.stdout:
            raise RuntimeError(result.stdout + result.stderr)
    for name in NAMES:
        path = DESTINATION / (name + ".scsyndef")
        data = path.read_bytes()
        if not data.startswith(b"SCgf") or len(data) < 128:
            raise ValueError("sclang produced an invalid SynthDef: " + name)
        print(name, len(data), "bytes")

if __name__ == "__main__":
    main()
