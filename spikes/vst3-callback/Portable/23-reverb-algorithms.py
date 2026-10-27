# FoxDot-compatible FreeVerb plus two optional algorithm extensions.
Clock.bpm = 100
Scale.default.set("minor")

# FoxDot's usual room/mix controls use FreeVerb.
p1 >> saw([0, 2, 4, 7], dur=0.5, room=0.65, mix=0.2, reverb="freeverb")

# FreeVerb2 keeps a separate stereo reverb path.
p2 >> triangle([7, 4, 2, 0], dur=1, room=0.75, mix=0.3, reverb="freeverb2")

# GVerb adds a longer hall-style tail; revtime/spread are extension controls.
p3 >> pulse([0, 4, 7], dur=2, room=0.8, mix=0.25, reverb="gverb",
            revtime=3.5, spread=25)
