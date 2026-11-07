#!/usr/bin/env python3
"""Compile optional insert SynthDefs using pinned upstream class definitions."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
NAMES = ("JPverb", "Greyhole")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("sc3_plugins", type=Path)
    args = parser.parse_args()
    output = HERE / "Portable/SynthDefs"
    with tempfile.TemporaryDirectory(prefix="dexter-space-reverbs-") as temp:
        temp = Path(temp)
        extensions = temp / "data/SuperCollider/Extensions"
        extensions.mkdir(parents=True)
        for name in NAMES:
            shutil.copy2(args.sc3_plugins / "source/DEINDUGens/sc" / (name + ".sc"),
                         extensions)
        script = temp / "generate.scd"
        script.write_text("\n".join(
            (output / (name + ".scd")).read_text().strip() +
            ".writeDefFile(" + json.dumps(output.as_posix()) + ");"
            for name in NAMES) + "\n0.exit;\n")
        env = dict(os.environ, XDG_DATA_HOME=str(temp / "data"),
                   QT_QPA_PLATFORM="minimal", QT_QUICK_BACKEND="software",
                   QT_OPENGL="software", QTWEBENGINE_DISABLE_SANDBOX="1")
        result = subprocess.run(["sclang", str(script)], env=env,
                                capture_output=True, text=True, timeout=90)
        if result.returncode or "ERROR:" in result.stdout:
            raise RuntimeError(result.stdout + result.stderr)
        for name in NAMES:
            data = (output / (name + ".scsyndef")).read_bytes()
            assert data.startswith(b"SCgf") and len(data) > 128
            print(name, len(data), "bytes")

if __name__ == "__main__":
    main()
