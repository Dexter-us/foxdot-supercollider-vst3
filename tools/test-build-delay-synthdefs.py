#!/usr/bin/env python3
"""Regression tests for the bounded bundled-sclang delay build step."""
from pathlib import Path
import contextlib
import importlib.util
import io
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).with_name("build-delay-synthdefs.py")
SPEC = importlib.util.spec_from_file_location("build_delay_synthdefs", SCRIPT)
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


class DelaySynthDefBuilderTests(unittest.TestCase):
    def test_build_script_combines_both_sources_and_one_exit(self):
        with tempfile.TemporaryDirectory() as temp:
            source_dir = Path(temp) / "sources"
            source_dir.mkdir()
            destination = Path(temp) / "Portable" / "SynthDefs"
            destination.mkdir(parents=True)
            for name in BUILDER.SOURCES:
                (source_dir / f"{name}.scd").write_text(
                    f"{name} = thisProcess.nowExecutingPath.dirname;\n",
                    encoding="utf-8")

            script = BUILDER.make_build_script(destination, source_dir)

        self.assertEqual(script.count("0.exit;"), 1)
        self.assertEqual(script.count(destination.as_posix()), 2)
        self.assertNotIn("thisProcess.nowExecutingPath.dirname", script)
        for name in BUILDER.SOURCES:
            self.assertIn(name, script)

    def test_sclang_is_invoked_once_with_a_bounded_timeout(self):
        with tempfile.TemporaryDirectory() as temp:
            script = Path(temp) / "build.scd"
            script.write_text("0.exit;", encoding="utf-8")
            completed = subprocess.CompletedProcess(
                args=["sclang"], returncode=0, stdout="compiled\n", stderr="")
            with patch.object(BUILDER.subprocess, "run", return_value=completed) as run:
                log = BUILDER.run_sclang("sclang.exe", script, 300)

        run.assert_called_once()
        self.assertEqual(log, "compiled\n")
        self.assertEqual(run.call_args.kwargs["timeout"], 300)

    def test_timeout_surfaces_partial_sclang_output(self):
        error = subprocess.TimeoutExpired(
            cmd=["sclang"], timeout=42,
            output=b"Loading class library...\n",
            stderr=b"warning before stall\n")
        stdout = io.StringIO()
        stderr = io.StringIO()
        with tempfile.TemporaryDirectory() as temp:
            script = Path(temp) / "build.scd"
            with patch.object(BUILDER.subprocess, "run", side_effect=error):
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    with self.assertRaisesRegex(RuntimeError, "42 seconds"):
                        BUILDER.run_sclang("sclang.exe", script, 42)

        self.assertIn("Loading class library", stdout.getvalue())
        self.assertIn("warning before stall", stderr.getvalue())

    def test_timeout_limit_is_bounded(self):
        with self.assertRaises(SystemExit):
            with patch("sys.argv", [
                str(SCRIPT), "--sclang", "sclang.exe",
                "--output", "out", "--timeout-seconds", "601",
            ]):
                BUILDER.main()

    def test_mode_switches_use_numeric_selectors(self):
        multi = (
            BUILDER.SOURCE_DIR /
            "DexterMultiTapDelayFX.scd"
        ).read_text(encoding="utf-8")
        feedback = (
            BUILDER.SOURCE_DIR /
            "DexterFeedbackDelayFX.scd"
        ).read_text(encoding="utf-8")
        self.assertIn("Select.kr(mode, [1, 1, randomGate])", multi)
        self.assertIn(
            "Select.kr(fx_mode.round.clip(0, 4), [1, 1, 1, 1, chance])",
            feedback)
        self.assertNotIn("Select.kr(mode == ", multi + feedback)


if __name__ == "__main__":
    unittest.main()
