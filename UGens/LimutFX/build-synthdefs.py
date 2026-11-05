#!/usr/bin/env python3
"""Compile LimutFX player-effect SynthDefs with the installed sclang."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SYNTHDEFS = HERE / "Portable" / "SynthDefs"
NAMES = ("LimutPingPong", "LimutDuck")


def main():
    if not shutil.which("sclang"):
        raise SystemExit("sclang is required only to rebuild the shipped .scsyndef files")
    with tempfile.TemporaryDirectory(prefix="dexter-limutfx-") as directory:
        temp = Path(directory)
        extensions = temp / "data" / "SuperCollider" / "Extensions"
        extensions.mkdir(parents=True)
        shutil.copy2(SYNTHDEFS / "LimutFX.sc", extensions)
        script = temp / "build.scd"
        script.write_text(
            "\n".join(
                (SYNTHDEFS / (name + ".scd")).read_text().strip()
                + ".writeDefFile("
                + json.dumps(SYNTHDEFS.as_posix())
                + ");"
                for name in NAMES
            )
            + "\n0.exit;\n",
            encoding="utf-8",
        )
        env = dict(
            os.environ,
            XDG_DATA_HOME=str(temp / "data"),
            QT_QPA_PLATFORM="minimal",
            QT_QUICK_BACKEND="software",
            QT_OPENGL="software",
            QTWEBENGINE_DISABLE_SANDBOX="1",
        )
        result = subprocess.run(
            ["sclang", str(script)],
            env=env,
            capture_output=True,
            text=True,
            timeout=90,
        )
        if result.returncode or "ERROR:" in result.stdout:
            raise RuntimeError(result.stdout + result.stderr)
        for name in NAMES:
            data = (SYNTHDEFS / (name + ".scsyndef")).read_bytes()
            if not data.startswith(b"SCgf") or len(data) < 128:
                raise RuntimeError(f"{name} did not produce a valid SynthDef")
            print(f"{name}: {len(data)} bytes")


if __name__ == "__main__":
    main()
