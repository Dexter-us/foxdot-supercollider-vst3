# Example 216: Sardine-style probability and grouped bar gates.
#
# chance is deterministic per player cycle (and repeatable with Seed.default).
# loaf groups bars; on uses 1-based bar positions within that group.
Clock.bpm = 112
Seed.default = 216

# Play this bass only on bars 1 and 3 of each 4-bar group, with a 75% chance
# on each eligible player cycle.
p1 >> bass([0, 0, 7, 5], dur=1, sus=0.8, oct=3,
          chance="often", loaf=4, on=(1, 3), amp=0.38)

# on without loaf follows Sardine's every-Nth-bar form.
p2 >> play("x---o---", dur=0.5, sample=0, on=2,
           chance="sometimes", amp=0.22, pan=0.2)

# Remove chance= or loaf/on= to hear the uninterrupted pattern.
