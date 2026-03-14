#include <cstdio>
#include <iostream>
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
		printf("SEGFAULT caught - check pointer initialization\n");
		fflush(stdout);
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
		printf("Failed to connect to hand via serial\n");
		// return -1;
	}
	else
	{
		printf("Connected to Ability Hand successfully\n");
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
				printf("Timeout %lld\n", elapsed_ms);
			}
			else if (rc != ABH_SUCCESS)
			{
				printf("Error: %d\n", rc);
			}
			else
			{
				float q[NUM_CHANNELS] = {};
				hand.get_positions(q);
				printf("(");
				for(int i = 0; i < NUM_CHANNELS; i++)
				{
					printf("%f, ", q[i]);
				}
				printf(")\n");
				// printf("%0.2X\n", hand.get_hand_state().hot_cold_bitmask);
			}

			// Check elapsed time (optional 60 second limit)

			if (elapsed_sec >= 240.0f)
			{
				printf("Leaving\n");
				break;
			}
		}
	}

	// Calculate final elapsed time
	int64_t elapsed_ms = millis_now() - start_ms;
	float elapsed_sec = elapsed_ms / 1000.0f;

	// Get and print statistics
	auto stats = hand.get_statistics();

	printf("\n\n=== Exit Statistics ===\n");
	if (stats.num_writes > 0 && elapsed_sec > 0.0f)
	{
		float avg_frequency = (float)stats.num_successful_reads / elapsed_sec;
		printf("Rate: %.2f Hz\n", avg_frequency);
		printf("Duration: %.2f sec\n", elapsed_sec);
	}
	printf("Writes: %u\n", stats.num_writes);
	printf("Reads: %u\n", stats.num_successful_reads);
	printf("Packet Loss: %.2f%%\n", stats.packet_loss_percent);
	printf("=======================\n");

	hand.disconnect();
	return 0;
}
