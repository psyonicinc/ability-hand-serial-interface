import time
import math
import abh

hand = abh.AbilityHand()
if hand.init() != abh.ABH_SUCCESS:	#autoconnect with 460800, cross platform, iterates all available serial devices and connects to the first one
    print("Warning: failed to connect to hand")
else:
	print("Connected to Ability Hand successfully")

hand.set_motion_mode(abh.MotionMode.POSITION_MODE_TX1)

start = time.time()

positions = [0.0]*abh.NUM_CHANNELS
while True:
    elapsed = time.time() - start
    for ch in range(abh.NUM_CHANNELS):
        positions[ch] = ((math.sin(elapsed + ch) + 1) / 2) * 50 + 10
    positions[5] = -55.0

    hand.set_position_targets(positions)
    rc = hand.exchange_motion_data()

    if rc == abh.ABH_ERROR_TIMEOUT:
        print(f"Timeout at {elapsed:.3f}s")
    elif rc != abh.ABH_SUCCESS:
        print(f"Error: {rc}")

    if elapsed >= 5.0:
        break

elapsed = time.time() - start
stats = hand.get_statistics()

print("\n=== Exit Statistics ===")
if stats.num_writes > 0 and elapsed > 0:
    print(f"Rate:        {stats.num_successful_reads / elapsed:.2f} Hz")
    print(f"Duration:    {elapsed:.2f} sec")
print(f"Writes:      {stats.num_writes}")
print(f"Reads:       {stats.num_successful_reads}")
print(f"Packet Loss: {stats.packet_loss_percent:.2f}%")
print("=======================")

hand.disconnect()
