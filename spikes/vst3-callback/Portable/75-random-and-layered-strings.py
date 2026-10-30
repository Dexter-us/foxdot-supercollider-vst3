# Example 75: curly braces choose a varied pattern; adjacent angle groups
# layer their symbols at matching beat positions.
Clock.bpm = 96

# Random choice is compiled as an eight-cycle pattern when Run is pressed.
p1 >> play("x{-o[--]}", dur=0.5, amp=0.3)

# x and - sound together at the start of each four-step group.
p2 >> play("<x   ><-   >", dur=0.5, amp=0.16, pan=0.15)

# Identical symbols may be layered; the engine assigns distinct MIDI note
# outputs for simultaneous sample voices while both still play the x sample.
p3 >> play("<x   ><x   >", dur=0.5, amp=0.07, pan=-0.15)
