import sys
import abh

hand = abh.AbilityHand()
if hand.init(460800) != abh.ABH_SUCCESS:
    print("Failed to connect to hand")
    sys.exit(-1)

hand.set_motion_mode(abh.MotionMode.POSITION_MODE_TX1)

rc, responses = hand.command_handling("R#", 7, 2000)
if rc == abh.ABH_SUCCESS:
    for r in responses:
        print(r)
else:
    print(f"error {rc}")

hand.disconnect()
