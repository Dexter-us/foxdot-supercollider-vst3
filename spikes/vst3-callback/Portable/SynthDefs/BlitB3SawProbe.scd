// Source used to compile BlitB3SawProbe.scsyndef with the SC3plugins
// AntiAliasingOscillators class wrapper available on the sclang class path.
SynthDef(\BlitB3SawProbe, { |out=0, freq=440, gate=1, amp=0.18, leak=0.99|
    var env = EnvGen.kr(Env.asr(0.01, 1, 0.12), gate);
    var sig = BlitB3Saw.ar(freq, leak);
    Out.ar(out, (sig * env * amp) ! 2);
}).writeDefFile(".");
0.exit;
