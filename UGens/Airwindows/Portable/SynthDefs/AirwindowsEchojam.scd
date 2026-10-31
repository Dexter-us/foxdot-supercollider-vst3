// SynthDef source for Dexter U.S. FoxDot's userSynth loader.
// Echojam.ar is the original Airwindows Echojam DSP, wrapped as an SC UGen.
SynthDef(\AirwindowsEchojam, {
    |out=0, freq=440, rate=1, sus=1, unused4=0, unused5=0,
     wetbus=16, wetgain=0, wetcutoff=20000, trigger=0, amp=1,
     vib=0.29, depth=0.32, lforate=0.48, lfowidth=0.5,
     rq=0.5, fmod=0, blur=0.62, cutoff=20000, atk=0.01, rel=2.5,
     beat_dur=0.5, buf=0, gate=0, pan=0|
    var envelope, mono, stereo, processed, output;
    envelope = EnvGen.kr(Env.asr(atk, 1, rel), gate);
    mono = SinOsc.ar(freq) * amp * envelope;
    stereo = Pan2.ar(mono, pan);
    processed = Echojam.ar(
        stereo[0], stereo[1], vib.clip(0, 1), depth.clip(0, 1),
        lforate.clip(0, 1), blur.clip(0, 1)
    );
    // The VST voice bus is mono; the UGen itself retains both stereo outputs.
    output = (processed[0] + processed[1]) * 0.5;
    Out.ar(out, output);
})
