# Example 221: bounded chance and bar gates inside a Sardine-style @swim.
# The finite cycle expansion keeps these gates in the native player scheduler.
Clock.bpm = 112

@swim(cycles=16, period=1)
def gated_groove(p=1, i=0):
    # Bass only on bars 1 and 3 of each four-bar phrase, with a 50% chance.
    p1 >> bass([0], dur=1, sus=0.8, oct=5, amp=0.3,
               chance=0.5, loaf=4, on=(1, 3))

    # The drums play with Sardine's "often" probability, on every second bar.
    p2 >> play("x---", dur=0.25, sample=0, amp=0.16,
               chance="often", on=2)

    again(gated_groove, p=p, i=i + 1)
