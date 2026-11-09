// Example 223 — use the SuperCollider tab and press Run.
// Keep the host's audio processing enabled. Do not boot another SC server.
// The bundled interpreter uses the existing VST engine.
(
Routine({
    SynthDef(\dexterHostBridge223, { |out=0, freq=330, amp=0.10, gate=1|
        var env = EnvGen.kr(Env.asr(0.02, 1, 0.15), gate, doneAction: 2);
        Out.ar(out, SinOsc.ar(freq) * env * amp ! 2);
    }).add;
    s.sync;
    x = Synth(\dexterHostBridge223, [\freq, 330]);
    0.7.wait;
    x.set(\freq, 440);
    0.7.wait;
    x.set(\freq, 550);
    0.7.wait;
    x.set(\gate, 0);
}).play(SystemClock);
)

// You may leave a FoxDot player running in the Python tab.
// SC Stop clears the SC group, not FoxDot's private nodes.
// No shutdown/reboot of FoxDot is required.
