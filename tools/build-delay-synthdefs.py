#!/usr/bin/env python3
"""Compile the built-in delay SynthDefs with the repository's pinned sclang."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "spikes/vst3-callback/Portable/SynthDefs"
SOURCES = {
    "DexterMultiTapDelayFX": {
        "fx_mix", "fx_mode", "fx_time", "fx_taps",
        "fx_feedback", "fx_probability", "fx_bpm",
    },
    "DexterFeedbackDelayFX": {
        "fx_mix", "fx_time", "fx_feedback", "fx_mode",
        "fx_pitch", "fx_interval", "fx_probability",
    },
}


def compile_one(name: str, destination: Path, sclang: str) -> Path:
    source = (SOURCE_DIR / f"{name}.scd").read_text(encoding="utf-8")
    marker = "thisProcess.nowExecutingPath.dirname"
    if marker not in source:
        raise ValueError(f"{name}.scd is missing the output-directory marker")
    script = source.replace(marker, '"' + destination.as_posix() + '"')
    with tempfile.TemporaryDirectory(prefix="dexter-delay-scd-") as temp:
        build_file = Path(temp) / "build.scd"
        build_file.write_text(script + "\n0.exit;\n", encoding="utf-8")
        env = dict(os.environ, QT_QPA_PLATFORM="minimal",
                   QT_QUICK_BACKEND="software", QT_OPENGL="software",
                   QTWEBENGINE_DISABLE_SANDBOX="1")
        result = subprocess.run(
            [sclang, str(build_file)], capture_output=True, text=True,
            env=env, timeout=180, cwd=str(Path(sclang).resolve().parent))
    log = result.stdout + result.stderr
    if result.returncode or "ERROR:" in log:
        raise RuntimeError(f"sclang failed compiling {name}:\n{log[-5000:]}")
    output = destination / f"{name}.scsyndef"
    data = output.read_bytes() if output.is_file() else b""
    if not data.startswith(b"SCgf") or name.encode() not in data:
        raise RuntimeError(f"sclang did not produce valid {name}.scsyndef")
    missing = [control for control in SOURCES[name]
               if control.encode() not in data]
    if missing:
        raise RuntimeError(f"{name} is missing controls: {', '.join(missing)}")
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sclang", default=shutil.which("sclang"), required=False)
    parser.add_argument("--output", type=Path, required=True,
                        help="SynthDefs directory inside the VST3 bundle")
    args = parser.parse_args()
    if not args.sclang:
        raise SystemExit("Pass the pinned SuperCollider 3.15 sclang executable")
    args.output.mkdir(parents=True, exist_ok=True)
    for name in SOURCES:
        path = compile_one(name, args.output, args.sclang)
        print(f"PASS {path.name}: {path.stat().st_size} bytes")
    validator = Path(__file__).with_name("validate-delay-synthdefs.py")
    result = subprocess.run(
        [sys.executable, str(validator), str(args.output)],
        capture_output=True, text=True, timeout=30)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
