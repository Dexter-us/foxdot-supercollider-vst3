# Example 153: keep sample strings separate from numeric note data.
Clock.bpm = 96
Scale.default = Scale.minor

sample_symbols = P["x", "o", "-", "o"].str()
degree_text = P[0, 2, 4, 6].str()

# These are text values, not sample symbols; map them back before playing notes.
numeric_degrees = degree_text.map(int)
p1 >> play(sample_symbols, dur=0.5, amp=0.16)
p2 >> triangle(numeric_degrees, dur=0.5, oct=4, amp=0.18, pan=0.25)
