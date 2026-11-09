#!/usr/bin/env python3
"""Run the actual C++ bootstrap with installed sclang against our UDP transport.

This validates language startup/OSC bytecode exchange, NOT DSP or the pinned
Windows interpreter. The required Windows test checks those separately.
"""
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "spikes/vst3-callback"


def wait_for(predicate, seconds, message):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    raise AssertionError(message)


def osc_string(value):
    data = value.encode() + b"\0"
    return data + b"\0" * (-len(data) % 4)


def main():
    sclang = shutil.which("sclang")
    if not sclang:
        raise RuntimeError("sclang is required; no skipped test is counted as a pass")
    install = Path(sclang).resolve().parents[1]
    classes = install / "share/SuperCollider/SCClassLibrary"
    assert classes.is_dir(), classes
    with tempfile.TemporaryDirectory(prefix="dexter-sclang-bootstrap-") as tmp:
        tmp = Path(tmp)
        fixture = tmp / "fixture"
        subprocess.run([
            "g++", "-std=c++17", "-O1", "-pthread",
            str(ROOT / "tools/test-sclang-host-bridge.cpp"),
            str(CPP / "sclang_host_bridge.cpp"), "-o", str(fixture),
        ], check=True)
        runtime = (CPP / "sclang_runtime_win32.cpp").read_text()
        expression = runtime.split("const auto bootstrap =\n", 1)[1].split(
            ";\n    if (!writeTextFile", 1)[0]
        generator = tmp / "generate.cpp"
        generator.write_text(
            '#include <iostream>\n#include <string>\n#include <cstdlib>\n'
            'int main(int, char** argv) { int serverPort=std::atoi(argv[1]); '
            'int firstUserAudioBus=512; double sampleRate=48000; std::cout << '
            + expression + "; }\n"
        )
        subprocess.run(["g++", "-std=c++17", str(generator), "-o", str(tmp / "generate")],
                       check=True)
        fixture_log = tmp / "fixture.log"
        language_log = tmp / "sclang.log"
        helper = language = None
        try:
            with fixture_log.open("w") as output:
                helper = subprocess.Popen([str(fixture), "--fixture"],
                                          stdin=subprocess.PIPE, stdout=output,
                                          stderr=subprocess.STDOUT)
            wait_for(lambda: "FIXTURE_PORT=" in fixture_log.read_text(), 5,
                     "UDP fixture did not start")
            server_port = int(re.search(r"FIXTURE_PORT=(\d+)", fixture_log.read_text())[1])
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                probe.bind(("127.0.0.1", 0))
                language_port = probe.getsockname()[1]
            bootstrap = tmp / "bootstrap.scd"
            bootstrap.write_bytes(subprocess.check_output(
                [str(tmp / "generate"), str(server_port)]))
            config = tmp / "sclang.yaml"
            config.write_text(
                f'includePaths:\n  - "{classes}"\nexcludePaths: []\nexcludeDefaultPaths: true\n')
            env = dict(os.environ, QT_QPA_PLATFORM="minimal",
                       QT_QUICK_BACKEND="software", QT_OPENGL="software",
                       QTWEBENGINE_DISABLE_SANDBOX="1",
                       QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu")
            with language_log.open("w") as output:
                language = subprocess.Popen([
                    sclang, "-D", "-u", str(language_port), "-l", str(config),
                    str(bootstrap),
                ], stdin=subprocess.DEVNULL, stdout=output,
                   stderr=subprocess.STDOUT, env=env)
            wait_for(lambda: "DEXTER_SCLANG_READY" in language_log.read_text(), 20,
                     "Actual sclang did not complete the bootstrap handshake")
            source = tmp / "test.scd"
            source.write_text("""(
Routine({
    SynthDef(\\dexterBootstrapCheck, {
        Out.ar(0, SinOsc.ar(997) * 0.1 ! 2);
    }).add;
    s.sync;
    x = Synth(\\dexterBootstrapCheck);
}).play(SystemClock);
)
""")
            message = (osc_string("/dexter/evalFile") + osc_string(",s") +
                       osc_string(str(source)))
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(message, ("127.0.0.1", language_port))
            wait_for(lambda: "LANGUAGE_SYNTH_REQUEST_RECEIVED" in fixture_log.read_text(),
                     5, "sclang did not send a synth request after SynthDef.add and s.sync")
            assert "SYNTHDEF_BYTECODE_RECEIVED" in fixture_log.read_text()
            example = CPP / "Portable/223-supercollider-host-bridge-check.scd"
            before = fixture_log.read_text().count("LANGUAGE_SYNTH_REQUEST_RECEIVED")
            message = (osc_string("/dexter/evalFile") + osc_string(",s") +
                       osc_string(str(example)))
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(message, ("127.0.0.1", language_port))
            wait_for(lambda: fixture_log.read_text().count(
                "LANGUAGE_SYNTH_REQUEST_RECEIVED") > before, 5,
                "Example 223 did not compile and send its synth")
            wait_for(lambda: fixture_log.read_text().count("COMMAND /n_set ") >= 3, 5,
                     "Example 223 did not send frequency changes and gate release")
            # CmdPeriod normally frees non-permanent OSC responders. The
            # editor's private control channel must survive its own Stop.
            for iteration in range(2):
                before_stop = fixture_log.read_text().count("COMMAND /g_freeAll ")
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.sendto(osc_string("/dexter/stop") + osc_string(","),
                                  ("127.0.0.1", language_port))
                wait_for(lambda: fixture_log.read_text().count(
                    "COMMAND /g_freeAll ") > before_stop, 5,
                    "Stop did not reach the server")
                marker = f"DEXTER_AFTER_STOP_{iteration}"
                source.write_text(f'"{marker}".postln;\n')
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.sendto(osc_string("/dexter/evalFile") + osc_string(",s") +
                                  osc_string(str(source)), ("127.0.0.1", language_port))
                wait_for(lambda: marker in language_log.read_text(), 5,
                         "Editor evaluation handler disappeared after CmdPeriod/Stop")
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(osc_string("/dexter/quit") + osc_string(","),
                              ("127.0.0.1", language_port))
            assert language.wait(timeout=5) == 0, "Quit failed after repeated Stop"
            assert "ERROR:" not in language_log.read_text()
            print("PASS: installed sclang + actual generated bootstrap + actual UDP transport: "
                  "status/notify, SynthDef bytecode, sync, allocated synth request; "
                  "Example 223 compilation and control/release messages; "
                  "evaluation/Stop/Quit responders survive repeated CmdPeriod")
            print("Scope: protocol test with a mock engine; not a DSP/Windows validation.")
        except Exception:
            print("Language log tail:\n" +
                  (language_log.read_text()[-3500:] if language_log.exists() else "(none)"))
            print("Fixture log tail:\n" + fixture_log.read_text()[-1500:])
            raise
        finally:
            if language is not None:
                language.terminate()
                try:
                    language.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    language.kill()
                    language.wait()
            if helper is not None:
                if helper.poll() is None:
                    helper.communicate(b"quit\n", timeout=3)


if __name__ == "__main__":
    main()
