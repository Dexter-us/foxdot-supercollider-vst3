# Example 148: repeat finite pitch patterns until a safe threshold is reached.
Clock.bpm = 100
Scale.default = Scale.minor

sum_line = P[1, 2, 3].limit(sum, 8)
count_line = P[0, 2, 4].limit(len, 8)
peak_line = P[1, 4, 7, 2].limit(max, 7)

p1 >> pluck(sum_line, dur=0.5, oct=4, amp=0.20)
p2 >> triangle(count_line, dur=0.5, oct=4, amp=0.12, pan=-0.25)
p3 >> bass(peak_line, dur=1, oct=3, amp=0.18, pan=0.25)
