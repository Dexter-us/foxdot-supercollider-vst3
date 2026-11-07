#!/usr/bin/env python3
"""Exercise exact embedded Python wrapper, without opening any audio files."""
import ast
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "spikes/vst3-callback/python_runtime.cpp").read_text()
api = source.split('static constexpr char api[] = R"PY(\n', 1)[1].split(')PY";', 1)[0]
checker = source.split('static constexpr char compatibilityChecker[] = R"PY(\n', 1)[1].split(')PY";', 1)[0]
wrapper = source.split("std::string makeEvaluationScript(", 1)[1].split("return script.str();", 1)[0]

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    script, result = directory / "script.py", directory / "result.txt"
    def evaluate(code):
        script.write_text(code, encoding="utf-8")
        values = {"api": api, "namespaceName": "disk_test",
                  "sourcePath": str(script), "resultPath": str(result)}
        parts = []
        for token in re.findall(r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\)|pythonStringTuple\([^)]*\))', wrapper):
            if token.startswith('"'): parts.append(ast.literal_eval(token))
            elif token.startswith("pythonStringTuple("): parts.append("()")
            else: parts.append(repr(values[token[len("pythonLiteral("):-1]]))
        exec("".join(parts), {"__name__": "__main__"})
        return result.read_text()
    output = evaluate(
        'DiskIn.play("DiskIO/drum-loop.wav", slot=8, loop=True, amp=2, pan=-1)\n'
        'VDiskIn.play("DiskIO/drum-loop.wav", rate=.0625)\n'
        'DiskOut.record(seconds=.05)\nDiskIn.stop(slot=8)\nDiskOut.stop()\n')
    assert output.startswith("OK\t0\t0\t5\n"), output
    assert "\nD\t1\t7\t1.0\t2.0\t-1.0\t0.0\t1\t" in output
    assert "\nD\t3\t0\t1.0\t0.0\t0.0\t0.05\t0\t-\n" in output
    for code in (
        'DiskIn.play("../outside.wav")', 'DiskIn.play("C:/outside.wav")',
        'DiskIn.play("/outside.wav")', 'DiskIn.play("a\\x00b.wav")',
        'DiskIn.play("ok.wav", slot=9)', 'DiskIn.play("ok.wav", slot=True)',
        'DiskIn.play("ok.wav", rate=2)', 'VDiskIn.play("ok.wav", rate=-1)',
        'VDiskIn.play("ok.wav", rate=float("nan"))',
        'DiskOut.record(seconds=float("inf"))', 'DiskOut.record(seconds=0)',
        'DiskOut.record(seconds=3601)', 'DiskIn.play("ok.wav", loop=1)',
        'DiskIn.play("ok.wav", amp=3)', 'DiskIn.play("ok.wav", pan=-2)',
        '\n'.join(['DiskOut.stop()'] * 17),
    ):
        output = evaluate(code)
        assert output.startswith("ERROR\n"), (code, output)
    # The exact checker reports through globals initialized by its C++ wrapper.
    # Compile it here; all Portable examples are also checked by test-pattern-api.
    compile(checker, "<exact compatibility checker>", "exec")
print("PASS exact embedded DiskIO API/wire, safe arguments and 16-operation bound")
