#include <iostream>
#include <chrono>
#include <thread>

#include "AbilityHand.h"

int main()
{
    AbilityHand hand;
    if(hand.init(460800) != ABH_SUCCESS)
    {
        std::cout << "Failed to connect to hand\n";
        return -1;
    }
    std::cout << "Connected to Ability Hand successfully\n";


    // A small, visually distinctive sequence
    struct {AbilityHand::GripCommand grip; const char* name; } sequence[] = {
        {AbilityHand::GRIP_OPEN,              "Open"},
        {AbilityHand::GRIP_POWER,             "Power"},
        {AbilityHand::GRIP_OPEN,              "Open"},
        {AbilityHand::GRIP_SIGN_OF_THE_HORNS, "Sign of the Horns"},
        {AbilityHand::GRIP_OPEN,              "Open"},
    };

    for (auto& step: sequence)
    {
        int rc = hand.cmd_grip(step.grip, 0xFF, 100);
        std::cout << step.name << " -> rc=" << rc;

        if (rc == ABH_SUCCESS)
        {
            float q[NUM_CHANNELS] = {};
            hand.get_positions(q);
            std::cout << " pos[0]=" << q[0]; 
        }
        std::cout << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }

    hand.disconnect();
    return 0;

}