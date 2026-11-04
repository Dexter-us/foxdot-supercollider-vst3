# Example 182: PShift is a FoxDot note control; select p1 to expose its slider.
# Drag slowly to retune active audio voices. For MIDI output, the new shift
# applies to subsequent note-ons. Releasing writes a scalar into this call.
p1 >> pluck(P[0, 4, 7, 12], dur=0.5,
            pshift=var([-12, 0, 7, 12], [2, 2, 2, 2]),
            amp=0.24)
