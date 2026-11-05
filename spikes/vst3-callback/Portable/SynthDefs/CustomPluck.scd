(
SynthDef(\CustomPluck, { |out=0, freq=440, amp=1, gate=0, atk=0.01, rel=0.18,
    brightness=0, sub=0|
    var envelope, sine, bright, signal;
    envelope = EnvGen.kr(Env.asr(atk, 1, rel), gate);
    sine = SinOsc.ar(freq);
    bright = Saw.ar(freq);
    signal = XFade2.ar(sine, bright,
        (brightness.clip(0, 1) * 2) - 1);
    signal = signal + (SinOsc.ar(freq * 0.5) * sub.clip(0, 0.5));
    signal = signal * amp * envelope;
    // Mono is fine; the plugin handles player panning.
    Out.ar(out, signal);
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
)
