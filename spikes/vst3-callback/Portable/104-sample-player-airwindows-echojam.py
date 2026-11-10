# Example 104: change Echojam using the fx_* arguments on p1.
# Install the Airwindows UGen pack and AirwindowsEchojam.scsyndef first.
Clock.bpm = 100

# These four Echojam controls are fixed values from 0 to 1. Edit them and
# re-run the script to update the insert; sample rhythm and slicing stay intact.
p1 >> play("x-o-oooxxx----", dur=0.2, sample=0,
          start=P[0.0, 0.15, 0.30, 0.45],
          end=P[0.28, 0.55, 0.72, 0.95],
          effect="AirwindowsEchojam",
          fx_mix=0.68, fx_vib=0.62, fx_depth=0.72, fx_lforate=0.38,
          fx_blur=0.82,
          amp=0.28)

# Keep the loop dry for comparison; Echojam is only on p1.
p2 >> play("oxxxoo", dur=2, sus=0.8, sample=0,
          start=linvar([0.05, 0.55], [2, 2]),
          end=linvar([0.55, 0.95], [2, 2]),
          loop=True, amp=0.12, pan=0.25)
