#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <math.h>
#include "AbilityHand.h"
#include "udp.h"


// Global flag for signal handling
volatile sig_atomic_t g_running = 1;

static int64_t millis_now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// Signal handler for Ctrl+C and segfaults
void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		g_running = 0;
	}
	else if (signal == SIGSEGV)
	{
		std::cout << "SEGFAULT caught - check pointer initialization\n" << std::flush;
		exit(1);
	}
}

int main()
{
	// Register signal handlers
	signal(SIGINT, signal_handler);
	signal(SIGSEGV, signal_handler);

	// Create and initialize AbilityHand
	AbilityHand hand;
	if (hand.init_udp("10.0.4.140", 5067) != ABH_SUCCESS)
	{
		std::cout << "Failed to connect to hand via serial\n";
		// return -1;
	}
	else
	{
		std::cout << "Connected to Ability Hand successfully\n";
	}

	// Set motion control mode
	// hand.set_motion_mode(AbilityHand::POSITION_MODE_TX1);
	hand.set_short_mode(AbilityHand::DUMMY_READ_TX1);

	// Set position targets (in degrees)
	float positions[NUM_CHANNELS] = {70.f, 10.f, 10.f, 10.f, 10.f, -50.f};

	// Timing for statistics
	int64_t start_ms = millis_now();

	// Main control loop
	uint64_t comm_ts = 0;
	while(g_running)
	{
		int64_t elapsed_ms = millis_now() - start_ms;

		if(elapsed_ms - comm_ts > 10)
		{
			comm_ts = elapsed_ms;
			float elapsed_sec = elapsed_ms / 1000.0f;

			// Exchange data with hand
			int rc = hand.exchange_short_msg(1);

			if (rc == ABH_ERROR_TIMEOUT)
			{
				std::cout << "Timeout " << elapsed_ms << "\n";
			}
			else if (rc != ABH_SUCCESS)
			{
				std::cout << "Error: " << rc << "\n";
			}
			else
			{
				float q[NUM_CHANNELS] = {};
				hand.get_positions(q);
				std::cout << "(";
				for(int i = 0; i < NUM_CHANNELS; i++)
				{
					std::cout << q[i] << ", ";
				}
				std::cout << ")\n";
				// std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)hand.get_hand_state().hot_cold_bitmask << "\n";
			}

			// Check elapsed time (optional 60 second limit)

			if (elapsed_sec >= 240.0f)
			{
				std::cout << "Leaving\n";
				break;
			}
		}
	}

	// Calculate final elapsed time
	int64_t elapsed_ms = millis_now() - start_ms;
	float elapsed_sec = elapsed_ms / 1000.0f;

	// Get and print statistics
	auto stats = hand.get_statistics();

	std::cout << std::fixed << std::setprecision(2);
	std::cout << "\n\n=== Exit Statistics ===\n";
	if (stats.num_writes > 0 && elapsed_sec > 0.0f)
	{
		float avg_frequency = (float)stats.num_successful_reads / elapsed_sec;
		std::cout << "Rate: " << avg_frequency << " Hz\n";
		std::cout << "Duration: " << elapsed_sec << " sec\n";
	}
	std::cout << "Writes: " << stats.num_writes << "\n";
	std::cout << "Reads: " << stats.num_successful_reads << "\n";
	std::cout << "Packet Loss: " << stats.packet_loss_percent << "%\n";
	std::cout << "=======================\n";

	hand.disconnect();
	return 0;
}
