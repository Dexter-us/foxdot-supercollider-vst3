# Example 103: TapeDelay2 processes the sample player as a persistent insert.
# Install the Airwindows UGen pack and AirwindowsTapeDelay2.scsyndef first.
Clock.bpm = 100

# Each WAV hit selects a different slice, then the player's summed audio
# passes through TapeDelay2. The sample pattern and slice controls stay intact.
p1 >> play("x-o-oooxxx----", dur=0.2, sample=0,
          start=P[0.0, 0.15, 0.30, 0.45],
          end=P[0.28, 0.55, 0.72, 0.95],
          effect="AirwindowsTapeDelay2", amp=0.28)

# Keep the loop dry as a comparison; only p1 uses the Airwindows insert.
p2 >> play("oxxxoo", dur=2, sus=0.8, sample=0,
          start=linvar([0.05, 0.55], [2, 2]),
          end=linvar([0.55, 0.95], [2, 2]),
          loop=True, amp=0.12, pan=0.25)
