#include <iostream>
#include "serial.h"
#include "PPP.h"
#include "abh_communication.h"

int main() 
{
	Serial s;
	if(s.autoconnect(460800) == false)
		return -1;
	abh_api_t abh = {};
	abh.address = 0x50;
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		abh.q_desired[ch] = (int16_t)(120.f * (32767.f/150.f));
	}
	
	
    return 0;
}
