// True two-input vocoder. inbus is the carrier; modbus is the modulator.
// The embedded FoxDot player-effect chain currently supplies only one player
// bus, so this UGen is packaged for explicit two-bus SynthDefs, not advertised
// as a one-input effect insert.
SynthDef(\LimutVocoder, { |out=0, inbus=16, modbus=18, mix=0.75, low=120, high=9000, attack=0.005, release=0.08, tilt=0, boost=1.5|
    var carrier = In.ar(inbus, 2);
    var modulator = In.ar(modbus, 2);
    var processed = LimutVocoder.ar(
        carrier[0], carrier[1], modulator[0], modulator[1],
        mix, low, high, attack, release, tilt, boost);
    Out.ar(out, processed);
}).writeDefFile;
