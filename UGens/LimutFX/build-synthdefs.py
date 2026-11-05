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
SIDECHAIN_SYNTHDEFS = HERE / "Portable" / "Sidechain" / "SynthDefs"
NAMES = (
    "LimutPingPong", "LimutDuck", "LimutDisperser", "LimutFuzz",
    "LimutShifter", "LimutShimmer", "LimutGatedVerb",
    "LimutCompressor", "LimutVocoder",
)


def main():
    if not shutil.which("sclang"):
        raise SystemExit("sclang is required only to rebuild the shipped .scsyndef files")
    with tempfile.TemporaryDirectory(prefix="dexter-limutfx-") as directory:
        temp = Path(directory)
        extensions = temp / "data" / "SuperCollider" / "Extensions"
        extensions.mkdir(parents=True)
        shutil.copy2(SYNTHDEFS / "LimutFX.sc", extensions)
        SIDECHAIN_SYNTHDEFS.mkdir(parents=True, exist_ok=True)
        # sclang's stdin mode parses each line as a standalone REPL input.
        # Keep every complete SynthDef expression on one line so multiline
        # source is not evaluated as a sequence of incomplete fragments.
        expressions = []
        for name in NAMES:
            source = (SYNTHDEFS / (name + ".scd")).read_text(encoding="utf-8")
            source = "\n".join(line.split("//", 1)[0]
                               for line in source.splitlines())
            source = " ".join(source.split())
            if source.endswith(";"):
                source = source[:-1]
            destination = (
                SIDECHAIN_SYNTHDEFS if name == "LimutVocoder" else SYNTHDEFS
            )
            expressions.append(
                source + ".writeDefFile("
                + json.dumps(destination.as_posix()) + ");"
            )
        script = " ".join(expressions) + " 0.exit;\n"
        env = dict(
            os.environ,
            XDG_DATA_HOME=str(temp / "data"),
            QT_QPA_PLATFORM="minimal",
            QT_QUICK_BACKEND="software",
            QT_OPENGL="software",
            QTWEBENGINE_DISABLE_SANDBOX="1",
        )
        try:
            result = subprocess.run(
                ["sclang"],
                input=script,
                env=env,
                capture_output=True,
                text=True,
                timeout=int(os.environ.get("LIMUTFX_SYNTHDEF_TIMEOUT", "180")),
            )
        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or ""
            stderr = error.stderr or ""
            if isinstance(stdout, bytes):
                stdout = stdout.decode(errors="replace")
            if isinstance(stderr, bytes):
                stderr = stderr.decode(errors="replace")
            raise RuntimeError(
                "sclang timed out while compiling SynthDefs.\n"
                + stdout[-6000:] + stderr[-6000:]) from error
        if result.returncode or "ERROR:" in result.stdout:
            raise RuntimeError(result.stdout + result.stderr)
        for name in NAMES:
            destination = (
                SIDECHAIN_SYNTHDEFS if name == "LimutVocoder" else SYNTHDEFS
            )
            data = (destination / (name + ".scsyndef")).read_bytes()
            if not data.startswith(b"SCgf") or len(data) < 128:
                raise RuntimeError(f"{name} did not produce a valid SynthDef")
            print(f"{name}: {len(data)} bytes")


if __name__ == "__main__":
    main()
