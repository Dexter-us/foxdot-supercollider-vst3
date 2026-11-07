# Example 194: inspect the embedded Pattern API and normalize nested data.
Clock.clear()
Clock.bpm = 94
Scale.default = Scale.minor

wrapped = P[P[0, 3, 5]]
data_view = wrapped.get_data()
normalised = wrapped.make()
fresh_phrase = normalised.new(P[2, 4, 7])

# These return useful information to scripts/editor tools without shell output.
supported_methods = P[0].get_methods()
pattern_help = P[0].help()
default_behaviour = P[0].get_behaviour()

p1 >> pluck(normalised, dur=1, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(fresh_phrase, dur=0.5, oct=5, amp=0.09, pan=0.25)
