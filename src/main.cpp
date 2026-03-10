#include <cstdio>

#define TINYCSOCKET_IMPLEMENTATION

// Platform headers (must come before GL on Windows)
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#endif

// tinycsocket (must come before SDL - SDL redefines main to SDL_main)
#include "tinycsocket.h"

#include <iostream>
#include <csignal>
#include <chrono>
#include <math.h>
#include "AbilityHand.h"

// Global flag for signal handling
volatile sig_atomic_t g_running = 1;

// Signal handler for Ctrl+C
void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		g_running = 0;
	}
}

int main()
{
	// Register signal handler for Ctrl+C
	signal(SIGINT, signal_handler);

	// Create and initialize AbilityHand
	AbilityHand hand;
	if (hand.init(460800) != ABH_SUCCESS)
	{
		printf("Failed to connect to hand via serial\n");
		// return -1;
	}
	else
	{
		printf("Connected to Ability Hand successfully\n");
	}
	
	if (tcs_lib_init() != TCS_SUCCESS)
	{
		printf("Failed to initialize tinycsocket\n");
	}
	else
	{
		printf("Initialize tinycsocket library success\n");
	}

	

	// Set motion control mode
	hand.set_motion_mode(AbilityHand::POSITION_MODE_TX1);

	// Set position targets (in degrees)
	float positions[NUM_CHANNELS] = {70.f, 10.f, 10.f, 10.f, 10.f, -50.f};

	// Timing for statistics
	auto start_time = std::chrono::steady_clock::now();

	// Main control loop
	while(g_running)
	{
		auto current_time = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
		float elapsed_sec = elapsed_ms / 1000.0f;
		for(int ch = 0; ch < NUM_CHANNELS; ch++)
		{
			positions[ch] = ((sin(elapsed_sec+(float)ch)+1)/2)*50 + 10;
		}

		// Set desired positions
		positions[5] = -55.f;  // Override last channel
		hand.set_position_targets(positions);

		// Exchange data with hand
		int rc = hand.exchange_motion_data(5);

		if (rc == ABH_ERROR_TIMEOUT)
		{
			printf("Timeout\n");
		}
		else if (rc != ABH_SUCCESS)
		{
			printf("Error: %d\n", rc);
		}
		else
		{
			float q[NUM_CHANNELS] = {};
			hand.get_positions(q);
			// printf("%0.2X\n", hand.get_hand_state().hot_cold_bitmask);
		}
		// Check elapsed time (optional 60 second limit)

		if (elapsed_sec >= 60.0f)
		{
			break;
		}
	}

	// Calculate final elapsed time
	auto end_time = std::chrono::steady_clock::now();
	auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
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
