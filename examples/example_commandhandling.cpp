#include "AbilityHand.h"

/*
	Wo
	We16
	We46
	We47
*/
int main(void)
{
	AbilityHand hand;
	if (hand.init(460800) != ABH_SUCCESS)
	{
		printf("Failed to connect to hand\n");
		return -1;
	}
	hand.set_motion_mode(AbilityHand::POSITION_MODE_TX1);
	std::vector<std::string> responses;
	int rc = hand.command_handling("Rc", responses, 1);
	if(rc == 0)
	{
		for(size_t i = 0; i < responses.size(); i++)
		{
			printf("%s\n", responses[i].c_str());
		}
	}
	else
	{
		printf("error %d\n", rc);
	}

}