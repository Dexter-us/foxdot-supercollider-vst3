// Example 222 — run from the SuperCollider tab, not the FoxDot/Python tab.
// The variable s is the VST's existing embedded SuperCollider 3.15 server.
// The Run button sends this score to bundled headless sclang; no second
// scsynth/audio server is started.
(
Routine({
    SynthDef(\dexterSclangExample222, { |out=0, freq=220, amp=0.12|
        var env = EnvGen.kr(Env.perc(0.01, 1.4), doneAction: 2);
        var sig = Saw.ar(freq) * amp * env;
        Out.ar(out, sig ! 2);
    }).add;
    s.sync;
    x = Synth(\dexterSclangExample222, [\freq, 220, \amp, 0.12]);
}).play(SystemClock);
)

// This sends a short, self-terminating stereo tone to the VST's main output.
// Stop from the editor's Stop button to cancel routines and free server nodes.
