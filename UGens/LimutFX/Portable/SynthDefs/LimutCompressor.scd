// Stereo-linked Limut-style compressor insert.
// fx_ratio maps Limut's ratio dB value (3 dB ~= 1.41:1, 20 dB = 10:1).
SynthDef(\LimutCompressor, { |out=0, inbus=0, mix=0.5, threshold=(-24), ratioDb=6, attack=0.01, release=0.1, knee=3|
    var input = In.ar(inbus, 2);
    var compressed = LimutCompressor.ar(
        input[0], input[1], mix, threshold, ratioDb, attack, release, knee);
    Out.ar(out, compressed);
}).writeDefFile;
