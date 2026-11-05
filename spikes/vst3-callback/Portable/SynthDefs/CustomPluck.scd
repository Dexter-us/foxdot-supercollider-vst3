(
SynthDef(\CustomPluck, { |out=0, freq=440, amp=1, gate=0, atk=0.01, rel=0.18|
    var envelope, signal;
    envelope = EnvGen.kr(Env.asr(atk, 1, rel), gate);
    signal = SinOsc.ar(freq) * amp * envelope;
    // Mono is fine; the plugin handles player panning.
    Out.ar(out, signal);
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
)
