#!/usr/bin/env python3
"""Regression tests for the bounded bundled-sclang delay build step."""
from pathlib import Path
import contextlib
import importlib.util
import io
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).with_name("build-delay-synthdefs.py")
SPEC = importlib.util.spec_from_file_location("build_delay_synthdefs", SCRIPT)
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)
VALIDATOR_SCRIPT = Path(__file__).with_name("validate-delay-synthdefs.py")
VALIDATOR_SPEC = importlib.util.spec_from_file_location(
    "validate_delay_synthdefs", VALIDATOR_SCRIPT)
VALIDATOR = importlib.util.module_from_spec(VALIDATOR_SPEC)
VALIDATOR_SPEC.loader.exec_module(VALIDATOR)


class DelaySynthDefBuilderTests(unittest.TestCase):
    @staticmethod
    def _v3_fixture():
        def pstring(value):
            data = value.encode("utf-8")
            return struct.pack(">B", len(data)) + data

        body = (
            pstring("Fixture")
            + struct.pack(">i", 0)  # constant count
            + struct.pack(">i", 0)  # control/default-value count
            + struct.pack(">i", 0)  # named-control count
            + struct.pack(">i", 0)  # UGen count
            + struct.pack(">H", 0)  # variant count
            + struct.pack(">ii", 0, 0)  # default Reblock
            + struct.pack(">fi", 1.0, 0)  # default Resample
        )
        graph = struct.pack(">i", len(body) + 4) + body
        return b"SCgf" + struct.pack(">iH", 3, 1) + graph

    @staticmethod
    def _v2_fixture():
        def pstring(value):
            data = value.encode("utf-8")
            return struct.pack(">B", len(data)) + data

        graph = (
            pstring("Fixture")
            + struct.pack(">iiiiH", 0, 0, 0, 0, 0)
        )
        return b"SCgf" + struct.pack(">iH", 2, 1) + graph

    def test_validator_still_parses_v2_layout(self):
        graphs = VALIDATOR.parse_synthdef(self._v2_fixture())
        self.assertEqual(set(graphs), {"Fixture"})
        self.assertEqual(graphs["Fixture"], ({}, []))

    def test_validator_parses_v3_size_and_reblock_resample_fields(self):
        graphs = VALIDATOR.parse_synthdef(self._v3_fixture())
        self.assertEqual(set(graphs), {"Fixture"})
        controls, ugens = graphs["Fixture"]
        self.assertEqual(controls, {})
        self.assertEqual(ugens, [])

    def test_validator_rejects_incorrect_v3_graph_size(self):
        data = bytearray(self._v3_fixture())
        declared = struct.unpack_from(">i", data, 10)[0]
        struct.pack_into(">i", data, 10, declared + 1)
        with self.assertRaisesRegex(ValueError, "v3 graph size mismatch"):
            VALIDATOR.parse_synthdef(bytes(data))

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
        self.assertIn("var loop = LocalIn.ar(1)", multi)
        self.assertIn("DelayC.ar(source, 8.1, tapTime)", multi)
        self.assertNotIn("CombC.ar", multi)
        self.assertIn("LocalOut.ar(LeakDC.ar", multi)
        self.assertIn("fx_bpm.clip(0.001, 300)", multi)
        self.assertIn("Lag.kr(fx_feedback.clip(0, 0.92), 0.025)", multi)

    def test_feedback_delay_disables_regeneration_when_player_stops(self):
        processor = (
            BUILDER.ROOT / "spikes/vst3-callback/processor.cpp"
        ).read_text(encoding="utf-8")
        helper = (
            BUILDER.ROOT / "spikes/vst3-callback/player_effect_chain.h"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "playerEffectFeedbackControlSlot(graphDefinition.name)", processor)
        self.assertIn(
            'effectName == "DexterMultiTapDelayFX"', helper)
        self.assertIn(
            'effectName == "DexterFeedbackDelayFX"', helper)


if __name__ == "__main__":
    unittest.main()
