(
SynthDef(\CustomTone, { |out=0, freq=440, amp=1, gate=0, atk=0.01, rel=0.2,
    blend=0.25, cutoff=4000, resonance=0.25|
    var envelope, oscillator, filtered;
    envelope = EnvGen.kr(Env.asr(atk, 1, rel), gate);
    oscillator = (SinOsc.ar(freq) * (1 - blend)) +
        (Saw.ar(freq) * blend);
    filtered = RLPF.ar(oscillator, cutoff.clip(40, 18000),
        resonance.clip(0.05, 1));
    Out.ar(out, filtered * amp * envelope);
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
)
