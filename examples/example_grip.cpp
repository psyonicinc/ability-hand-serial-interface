#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <thread>
#include <random>

#include "AbilityHand.h"

// Global flag for signal handling
volatile sig_atomic_t g_running = 1;

static int64_t millis_now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void signal_handler(int signal)
{
    if(signal == SIGINT)
    {
        g_running = 0;
    }
    else if (signal == SIGSEGV)
    {
        std::cout << "SEGFAULT caught - check pointer initialization\n" << std::flush;
        exit(1);
    }
}

struct GripEntry
{
    AbilityHand::GripCommand grip;
    const char* name;
};

static const GripEntry kGrips[] = {
	{AbilityHand::GRIP_OPEN,              "Open"},
	{AbilityHand::GRIP_POWER,             "Power"},
	{AbilityHand::GRIP_KEY,               "Key"},
	{AbilityHand::GRIP_PINCH,             "Pinch"},
	{AbilityHand::GRIP_TRIPOD_OPENED,     "Tripod Opened"},
	{AbilityHand::GRIP_SIGN_OF_THE_HORNS, "Sign of the Horns"},
	{AbilityHand::GRIP_CYLINDER,          "Cylinder"},
	{AbilityHand::GRIP_MOUSE,             "Mouse"},
	{AbilityHand::GRIP_POWER_KEY_SWITCH,  "Power/Key Switch"},
	{AbilityHand::GRIP_POINT,             "Point"},
	{AbilityHand::GRIP_RUDE_POINT,        "Rude Point"},
	{AbilityHand::GRIP_HOOK,              "Hook"},
	{AbilityHand::GRIP_RELAX,             "Relax"},
	{AbilityHand::GRIP_SLEEVE,            "Sleeve"},
	{AbilityHand::GRIP_PEACE,             "Peace"},
	{AbilityHand::GRIP_TRIPOD_CLOSED,     "Tripod Closed"},
	{AbilityHand::GRIP_HANGLOOSE,         "Hangloose"},
	{AbilityHand::GRIP_HANDSHAKE,         "Handshake"},
	{AbilityHand::GRIP_FIXED_PINCH,       "Fixed Pinch"},
	{AbilityHand::GRIP_TRIGGER,           "Trigger"},
	{AbilityHand::GRIP_FINGER_WAVE,       "Finger Wave"},
};
static const int kNumGrips = sizeof(kGrips) / sizeof(kGrips[0]);
static const int kNumGripsToCycle = 5; 

int main()
{
    // Register signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

    // Create and initialize AbilityHand
    AbilityHand hand;
    if(hand.init(460800) != ABH_SUCCESS)
    {
        std::cout << "Failed to connect to hand\n";
        return -1;
    }
    std::cout << "Connected to Ability Hand successfully\n";

    // Timiing for statistics
    int64_t start_ms = millis_now();

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> grip_dist(0, kNumGrips - 1);

    // Cycle through 5 random grips
    for(int i = 0; i < kNumGripsToCycle && g_running; i++)
    {
        const GripEntry& entry = kGrips[grip_dist(rng)];

        abh_grip_t reply = {};
        int rc = hand.cmd_grip(entry.grip, 0xFF, reply, 100);

        std::cout << "[" << (i + 1) << "/" << kNumGripsToCycle << "] " << entry.name << " -> rc=" << rc;
        if (rc == ABH_SUCCESS)
        {
            std::cout << " handshake=" << reply.handshake_valid
                      << " status=0x" << std::hex << (int)reply.grip_status << std::dec
                      << " percent=" << (int)reply.grip_percent
                      << " idle=" << reply.idle;
        }
        std::cout << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
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