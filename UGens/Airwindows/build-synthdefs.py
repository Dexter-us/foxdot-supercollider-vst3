#!/usr/bin/env python3
"""Compile the two checked-in SynthDef sources without starting an SC server."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
SYNTHDEF_DIR = HERE / "Portable" / "SynthDefs"
SOURCES = (
    SYNTHDEF_DIR / "AirwindowsTapeDelay2.scd",
    SYNTHDEF_DIR / "AirwindowsEchojam.scd",
)


def main() -> int:
    sclang = shutil.which("sclang")
    if not sclang:
        raise SystemExit("sclang is needed only to rebuild the precompiled SynthDefs.")

    with tempfile.TemporaryDirectory(prefix="dexter-airwindows-sclang-") as temp:
        temp_path = Path(temp)
        extension_dir = temp_path / "xdg-data" / "SuperCollider" / "Extensions"
        extension_dir.mkdir(parents=True)
        shutil.copy2(
            SYNTHDEF_DIR / "AirwindowsUGens.sc",
            extension_dir / "AirwindowsUGens.sc",
        )

        generated_script = temp_path / "generate.scd"
        output_path = SYNTHDEF_DIR.as_posix()
        chunks = []
        for source_path in SOURCES:
            source = source_path.read_text(encoding="utf-8").strip()
            chunks.append(
                f"{source}.writeDefFile({json.dumps(output_path)});"
            )
        generated_script.write_text("\n".join(chunks) + "\n", encoding="utf-8")

        env = os.environ.copy()
        env["XDG_DATA_HOME"] = str(temp_path / "xdg-data")
        env["QT_QPA_PLATFORM"] = "minimal"
        env["QT_QUICK_BACKEND"] = "software"
        env["QT_OPENGL"] = "software"
        command = (
            f"thisProcess.interpreter.executeFile("
            f"{json.dumps(generated_script.as_posix())}); 0.exit;\n"
        )
        result = subprocess.run(
            [sclang],
            input=command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            timeout=90,
            check=False,
        )
        expected = (
            SYNTHDEF_DIR / "AirwindowsTapeDelay2.scsyndef",
            SYNTHDEF_DIR / "AirwindowsEchojam.scsyndef",
        )
        missing = [path.name for path in expected if not path.is_file()]
        if result.returncode != 0 or missing or "ERROR:" in result.stdout:
            print(result.stdout)
            raise SystemExit(
                "SynthDef compilation failed"
                + (f"; missing: {', '.join(missing)}" if missing else "")
            )
        for path in expected:
            if path.stat().st_size < 128:
                raise SystemExit(f"Generated SynthDef looks too small: {path}")
            print(f"Generated {path.name} ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
