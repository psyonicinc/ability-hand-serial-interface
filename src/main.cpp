#include <iostream>
#include "serial.h"
#include "PPP.h"
#include "abh_communication.h"

int dartt_to_ppp(buffer_t * in, ppp_buffer_t * out)
{
	out->buf = in->buf;
	out->length = in->len;
	out->size = in->size;
	return 0;
}

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
	unsigned char tx_mem[128] = {};
	buffer_t tx_frame;
	tx_frame.buf = tx_mem;
	tx_frame.len = 0;
	tx_frame.size = sizeof(tx_mem);
	abh_create_movement_frame(&abh, &tx_frame);
	
	ppp_buffer_t unstuff_alias;
	dartt_to_ppp(&tx_frame, &unstuff_alias);
	unsigned char stuff_mem[128] = {};
	ppp_buffer_t stuff_buf;
	stuff_buf.buf = stuff_mem;
	stuff_buf.length = 0;
	stuff_buf.size = sizeof(stuff_mem);
	int rc = PPP_stuff(&unstuff_alias, &stuff_buf);

	while(1)
	{
		s.write(stuff_buf.buf, (int)stuff_buf.length);
	}

    return 0;
}
