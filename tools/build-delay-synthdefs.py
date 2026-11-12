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
DEFAULT_TIMEOUT_SECONDS = 300
MAX_TIMEOUT_SECONDS = 600


def make_build_script(destination: Path, source_dir: Path = SOURCE_DIR) -> str:
    """Join all definitions into one script so sclang loads its class library once."""
    marker = "thisProcess.nowExecutingPath.dirname"
    output_path = '"' + destination.as_posix().replace('"', '\\"') + '"'
    scripts = []
    for name in SOURCES:
        source_path = source_dir / f"{name}.scd"
        source = source_path.read_text(encoding="utf-8")
        if marker not in source:
            raise ValueError(f"{source_path.name} is missing the output-directory marker")
        scripts.append(source.replace(marker, output_path))
    return "\n\n".join(scripts) + "\n0.exit;\n"


def _output_text(value) -> str:
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)


def run_sclang(sclang: str, build_file: Path, timeout_seconds: int) -> str:
    env = dict(os.environ, QT_QPA_PLATFORM="minimal",
               QT_QUICK_BACKEND="software", QT_OPENGL="software",
               QTWEBENGINE_DISABLE_SANDBOX="1")
    try:
        result = subprocess.run(
            [sclang, str(build_file)], capture_output=True, text=True,
            env=env, timeout=timeout_seconds,
            cwd=str(Path(sclang).resolve().parent))
    except subprocess.TimeoutExpired as exc:
        stdout = _output_text(exc.stdout)
        stderr = _output_text(exc.stderr)
        if stdout:
            print("Partial sclang stdout before timeout:\n" + stdout, file=sys.stdout)
        if stderr:
            print("Partial sclang stderr before timeout:\n" + stderr, file=sys.stderr)
        raise RuntimeError(
            f"pinned sclang did not finish compiling the delay SynthDefs "
            f"within {timeout_seconds} seconds; partial output is shown above"
        ) from exc
    return result.stdout + result.stderr


def validate_outputs(destination: Path) -> list[Path]:
    outputs = []
    for name, required_controls in SOURCES.items():
        output = destination / f"{name}.scsyndef"
        data = output.read_bytes() if output.is_file() else b""
        if not data.startswith(b"SCgf") or name.encode() not in data:
            raise RuntimeError(f"sclang did not produce valid {name}.scsyndef")
        missing = [control for control in required_controls
                   if control.encode() not in data]
        if missing:
            raise RuntimeError(
                f"{name} is missing controls: {', '.join(sorted(missing))}")
        outputs.append(output)
    return outputs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sclang", default=shutil.which("sclang"), required=False)
    parser.add_argument("--output", type=Path, required=True,
                        help="SynthDefs directory inside the VST3 bundle")
    parser.add_argument(
        "--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS,
        help=f"bounded timeout for the single sclang session (max {MAX_TIMEOUT_SECONDS})")
    args = parser.parse_args()
    if not args.sclang:
        raise SystemExit("Pass the pinned SuperCollider 3.15 sclang executable")
    if not 1 <= args.timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise SystemExit(
            f"--timeout-seconds must be between 1 and {MAX_TIMEOUT_SECONDS}")

    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dexter-delay-scd-") as temp:
        build_file = Path(temp) / "build.scd"
        build_file.write_text(
            make_build_script(args.output), encoding="utf-8")
        print(
            f"Compiling {len(SOURCES)} delay SynthDefs in one pinned sclang "
            f"session (timeout {args.timeout_seconds}s).",
            flush=True)
        log = run_sclang(args.sclang, build_file, args.timeout_seconds)

    if log:
        print(log, end="" if log.endswith("\n") else "\n")
    if "ERROR:" in log:
        raise RuntimeError(f"sclang reported a compile error:\n{log[-5000:]}")
    outputs = validate_outputs(args.output)
    for path in outputs:
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
