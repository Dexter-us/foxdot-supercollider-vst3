#!/usr/bin/env python3
"""Compile the stock-UGen granular insert without rebuilding SuperCollider."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "spikes/vst3-callback/Portable/SynthDefs/DexterGranularFX.scd"
OUTPUT = SOURCE.with_suffix(".scsyndef")


def compile_to(destination: Path, sclang: str) -> bytes:
    destination.mkdir(parents=True, exist_ok=True)
    text = SOURCE.read_text(encoding="utf-8")
    marker = "thisProcess.nowExecutingPath.dirname"
    if marker not in text:
        raise ValueError("Granular SynthDef source is missing its output marker")
    expression = text.replace(marker, json.dumps(destination.as_posix()))
    expression = " ".join(
        line for line in expression.splitlines()
        if not line.lstrip().startswith("//"))
    expression = " ".join(expression.split())
    env = dict(os.environ, QT_QPA_PLATFORM="minimal",
               QT_QUICK_BACKEND="software", QT_OPENGL="software")
    result = subprocess.run(
        [sclang], input=expression + "\n0.exit;\n",
        capture_output=True, text=True, env=env, timeout=120)
    output = result.stdout + result.stderr
    if result.returncode or "ERROR:" in output:
        raise RuntimeError("sclang failed to compile DexterGranularFX:\n" +
                           output[-6000:])
    binary = destination / "DexterGranularFX.scsyndef"
    data = binary.read_bytes() if binary.is_file() else b""
    if not data.startswith(b"SCgf") or len(data) < 128:
        raise RuntimeError("sclang did not produce a valid DexterGranularFX SynthDef")
    return data


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--sclang", default=shutil.which("sclang"))
    parser.add_argument("--check", action="store_true",
                        help="compile in a temp folder and compare with the checked-in SynthDef")
    args = parser.parse_args()
    if not args.sclang:
        raise SystemExit("sclang is required to compile or verify this SynthDef")
    if args.check:
        with tempfile.TemporaryDirectory(prefix="dexter-granular-def-") as folder:
            fresh = compile_to(Path(folder), args.sclang)
        checked = OUTPUT.read_bytes()
        if fresh != checked:
            raise SystemExit("Checked-in DexterGranularFX.scsyndef is stale")
        print("PASS: source recompiles to the checked-in SynthDef")
    else:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="dexter-granular-def-") as folder:
            fresh = compile_to(Path(folder), args.sclang)
        OUTPUT.write_bytes(fresh)
        print(f"Wrote {OUTPUT.name}: {len(fresh)} bytes")


if __name__ == "__main__":
    main()
