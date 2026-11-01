#!/usr/bin/env python3
"""Compile the vendored FoxDot graphs, adapting routing and pooled-note triggers.

Requires sclang only when regenerating the checked-in header, NOT for Windows
plugin builds. No server is booted. See foxdot-synths/README.md for differences.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "spikes/vst3-callback/foxdot-synths"
NAMES = "viola nylon arpy dab sawbass dbass prophet pads pasha ambi sinepad keys klank feel glass soft quin space scratch creep snick swell razz sitar star karp".split()
CONTROLS = "out freq rate sus unused4 unused5 wetbus wetgain wetcutoff trigger amp vib depth lforate lfowidth rq fmod blur cutoff atk rel beat_dur".split()
DEFAULTS = [0, 440, 0, 1, 0, 0, 0, 0, 20000, 0, 1, 0, .5, 1, .5, .5, 0, 1, 20000, .01, .04, 1]
CONTROL_MAP = {name: i for i, name in enumerate(CONTROLS)}


def adapt(name, original):
    match = re.search(r"\{\s*\|([^|]+)\|(.*)\}\s*\)\s*\.add\s*;?", original, re.S)
    if not match:
        raise ValueError(f"Cannot find SynthDef body for {name}")
    args, body = match.groups()
    defaults = dict(re.findall(r"(\w+)\s*=\s*([^,\s|]+)", args))
    declarations = []
    while True:
        declaration = re.match(r"\s*var\s+([^;]+);", body)
        if not declaration:
            break
        declarations.extend(x.strip() for x in declaration[1].split(","))
        body = body[declaration.end():]
    variables = list(dict.fromkeys(["c", "osc"] + list(defaults) + declarations))
    # The embedded host supplies frequency, not FoxDot's control-rate bus.
    body, count = re.subn(r"In\.kr\(bus(?:,\s*1)?\)", "c[1]", body)
    if count != 1:
        raise ValueError(f"Expected exactly one frequency-bus read for {name}")
    # Mono signal + host panning; preserve each source's Mix/gain arithmetic.
    body = re.sub(r"osc\s*=\s*Pan2\.ar\(osc,\s*pan\)\s*;", "", body)
    # Pooled graphs must not free their own nodes. All envelopes retrigger.
    body = re.sub(r"doneAction\s*:\s*[023]", "doneAction: 0", body)
    body = body.replace("EnvGen.ar(", "~foxEnv.value(")
    for rate in ("ar", "kr"):
        body = body.replace(f"XLine.{rate}(", f"~foxXLine{rate}.value(")
        body = body.replace(f"Line.{rate}(", f"~foxLine{rate}.value(")
    # Rand is normally sampled on s_new; pooled voices need a new value per note.
    body = re.sub(r"\bRand\(([^()]+)\)", r"TRand.kr(\1, c[9])", body)
    if name == "sawbass":
        body = body.replace("Pan2.ar((filter + filter2) * env * amp, pan)",
                            "(osc = (filter + filter2) * env * amp)")
    elif name == "prophet":
        body = body.replace("Pan2.ar(Mix(filter) * env * amp * 0.5, pan)",
                            "(osc = Mix(filter) * env * amp * 0.5)")
    output = r"""Out.ar(c[0], [osc, K2A.ar(MouseX.kr(0, 1, 0, 0.2)), K2A.ar(MouseY.kr(0, 1, 0, 0.2))]);
        Out.ar(c[6], (LPF.ar(osc, c[8]) * Lag.kr(c[7], 0.005)).dup)"""
    def replace_output(match):
        expression = match[1]
        prefix = expression + ";\n" if expression.startswith("(osc =") else ""
        if expression != "osc" and not prefix:
            raise ValueError(f"Unknown output expression in {name}: {expression}")
        return prefix + output
    body, count = re.subn(r"ReplaceOut\.ar\(bus,\s*(.+)\)", replace_output, body)
    if count != 1:
        raise ValueError(f"Expected one mono output for {name}")
    if name == "sawbass":
        # Upstream holds this filter ADSR's gate at one for the node lifetime.
        body = body.replace("Env.adsr(0.0, 0.5, 0.2, sus)",
                            "Env([0, 1, 0.2], [0, 0.5])")
    if name == "creep":
        # An exponential segment cannot reach zero. Preserve the rising
        # exponential, but make the source's final 1 ms fall linear.
        body = body.replace("levels: [0.0001, amp, 0]",
                            "levels: [0.0001, amp.max(1e-9), 1e-9]")
        body = body.replace("curve: 'exp'", "curve: ['exp', 'lin']")
    defaults_list = list(DEFAULTS)
    for arg, index in CONTROL_MAP.items():
        if arg in defaults:
            defaults_list[index] = float(defaults[arg])
    assignments = []
    for arg, default in defaults.items():
        value = "c[0]" if arg == "bus" else "0" if arg == "pan" else (
            f"c[{CONTROL_MAP[arg]}]" if arg in CONTROL_MAP else default)
        assignments.append(f"{arg} = {value};")
    symbols = ", ".join("\\" + n for n in CONTROLS)
    values = ", ".join(str(v) for v in defaults_list)
    return (
        f'SynthDef("Dexter{name.capitalize()}", {{\n'
        f'var {", ".join(variables)};\n'
        f'c = Control.names([{symbols}]).kr([{values}]);\n'
        "~foxTrigger = c[9];\n" + "\n".join(assignments) + "\n" + body +
        f'\n}}).writeDefFile(outDir);\n'
    )


HELPERS = r"""
~foxEnv = { |envelope, gate=1, levelScale=1, levelBias=0, timeScale=1, doneAction=0|
    EnvGen.ar(envelope, gate: ~foxTrigger, doneAction: 0)
};
~foxLinear = { |start=0, end=1, dur=1, doneAction=0|
    var env = Env([start, end], [dur], \lin);
    EnvGen.ar(env, ~foxTrigger)
};
~foxLinekr = { |start=0, end=1, dur=1, doneAction=0|
    EnvGen.kr(Env([start, end], [dur], \lin), ~foxTrigger)
};
~foxXLinear = { |start=1, end=2, dur=1, doneAction=0|
    EnvGen.ar(Env([start, end], [dur], \exp), ~foxTrigger)
};
~foxXLinekr = { |start=1, end=2, dur=1, doneAction=0|
    EnvGen.kr(Env([start, end], [dur], \exp), ~foxTrigger)
};
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=SOURCE.parent / "foxdot_synthdefs.h")
    parser.add_argument("--defs", type=Path, help="Keep individual compiled definitions here")
    options = parser.parse_args()
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        output = options.defs.resolve() if options.defs else temporary / "defs"
        output.mkdir(parents=True, exist_ok=True)
        script = "(\nvar outDir = " + json.dumps(str(output)) + ";\n" + HELPERS
        manifest = {}
        for name in NAMES:
            content = (SOURCE / "upstream" / (name + ".scd")).read_text()
            manifest[name] = hashlib.sha256(content.encode()).hexdigest()
            script += adapt(name, content)
        script += '"FOXDOT_BANK_COMPILED".postln;\n0.exit;\n)\n'
        script_path = temporary / "compile.scd"
        script_path.write_text(script)
        if options.defs:
            (output / "compile.scd").write_text(script)
        env = dict(os.environ, QT_QPA_PLATFORM="minimal", QT_QUICK_BACKEND="software",
                   QT_OPENGL="software", QTWEBENGINE_DISABLE_SANDBOX="1",
                   QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu")
        try:
            result = subprocess.run(["sclang"], env=env,
                                    input='thisProcess.interpreter.executeFile(' +
                                    json.dumps(str(script_path)) + '); 0.exit;\n',
                                    capture_output=True, text=True, timeout=30)
        except subprocess.TimeoutExpired as error:
            raise SystemExit("sclang failed to finish:\n" +
                             (error.stdout or b"").decode(errors="replace") +
                             (error.stderr or b"").decode(errors="replace"))
        if result.returncode or "FOXDOT_BANK_COMPILED" not in result.stdout:
            raise SystemExit(result.stdout + result.stderr)
        definitions = []
        for name in NAMES:
            blob = (output / f"Dexter{name.capitalize()}.scsyndef").read_bytes()
            if blob[:10] != b"SCgf" + struct.pack(">iH", 2, 1):
                raise ValueError("Unexpected SynthDef version/count")
            definitions.append(blob[10:])
        bank = b"SCgf" + struct.pack(">iH", 2, len(definitions)) + b"".join(definitions)
        rows = [", ".join(f"0x{v:02x}" for v in bank[i:i + 12])
                for i in range(0, len(bank), 12)]
        options.output.write_text(
            "// Generated by tools/build-foxdot-synth-bank.py; do not edit.\n"
            "static constexpr unsigned char kFoxDotSynthDefs[] = {\n" +
            "".join("    " + row + ",\n" for row in rows) + "};\n")
        (SOURCE / "source-sha256.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"Compiled {len(NAMES)} FoxDot definitions ({len(bank)} bytes)")


if __name__ == "__main__":
    main()
