# Example 103: change TapeDelay2 using the fx_* arguments on p1.
# Install the Airwindows UGen pack and AirwindowsTapeDelay2.scsyndef first.
Clock.bpm = 100

# Each WAV hit selects a different slice, then the player's summed audio
# passes through TapeDelay2. All fx_* values are fixed 0..1 controls. Edit them
# and re-run the script to update the effect without changing the sample pattern.
p1 >> play("x-o-oooxxx----", dur=0.2, sample=0,
          start=P[0.0, 0.15, 0.30, 0.45],
          end=P[0.28, 0.55, 0.72, 0.95],
          effect="AirwindowsTapeDelay2",
          fx_vib=0.72, fx_depth=0.64, fx_lforate=0.35,
          fx_lfowidth=0.42, fx_rq=0.18, fx_blur=0.76,
          amp=0.28)

# Keep the loop dry as a comparison; only p1 uses the Airwindows insert.
p2 >> play("oxxxoo", dur=2, sus=0.8, sample=0,
          start=linvar([0.05, 0.55], [2, 2]),
          end=linvar([0.55, 0.95], [2, 2]),
          loop=True, amp=0.12, pan=0.25)
