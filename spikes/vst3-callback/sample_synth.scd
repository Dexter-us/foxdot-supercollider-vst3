// Native SC buffer playback, with the plugin's existing private-bus ABI.
// No disk reader, external server, or third-party UGen is used at playback.
SynthDef(\DexterSample, {
    |out=0, freq=440, rate=1, sus=1, unused4=0, unused5=0,
     wetbus=16, wetgain=0, wetcutoff=20000, trigger=1, amp=1,
     vib=0, depth=0.5, lforate=1, lfowidth=0.5, rq=0.5, fmod=0,
     blur=1, cutoff=20000, atk=0.001, rel=0.005, beat_dur=0.5, buf=0|
    var start = Select.kr(rate < 0, [0, BufFrames.kr(buf) - 1]);
    var sound = LPF.ar(PlayBuf.ar(1, buf, rate * BufRateScale.kr(buf), 1, start, 0, 0), cutoff) * amp;
    Out.ar(out, [sound, K2A.ar(MouseX.kr(0, 1, 0, 0.2)), K2A.ar(MouseY.kr(0, 1, 0, 0.2))]);
    Out.ar(wetbus, (LPF.ar(sound, wetcutoff) * Lag.kr(wetgain, 0.005)).dup);
})
