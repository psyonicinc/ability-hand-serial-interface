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

void print_ppp_buf(ppp_buffer_t * in)
{
	printf("0x");
	for(int i = 0; i < in->length; i++)
	{
		printf("%0.2X", in->buf[i]);
	}
	printf("\n");
}

int main() 
{
	Serial s;
	if(s.autoconnect(460800) == false)
		return -1;
	abh_api_t abh = {};
	abh.address = 0x50;
	abh.command_header = FIXED_POSITION_CONTROL_TX1;
	unsigned char tx_mem[128] = {};
	buffer_t tx_frame;
	tx_frame.buf = tx_mem;
	tx_frame.len = 0;
	tx_frame.size = sizeof(tx_mem);
	abh_create_movement_frame(&abh, &tx_frame);

	ppp_buffer_t unstuff_alias;
	dartt_to_ppp(&tx_frame, &unstuff_alias);
	unsigned char stuff_mem[sizeof(tx_mem)*2+2] = {};
	ppp_buffer_t stuff_buf;
	stuff_buf.buf = stuff_mem;
	stuff_buf.length = 0;
	stuff_buf.size = sizeof(stuff_mem);
	int rc = PPP_stuff(&unstuff_alias, &stuff_buf);

	uint8_t rx_mem[256] = {};
	ppp_buffer_t rx_buffer_alias;	//stuffed
	rx_buffer_alias.buf = rx_mem;
	rx_buffer_alias.length = 0;
	rx_buffer_alias.size = sizeof(rx_mem);
	uint8_t rx_unstuff_mem[128] = {};
	ppp_buffer_t rx_ppp_unstuff_alias;
	rx_ppp_unstuff_alias.buf = rx_unstuff_mem;
	rx_ppp_unstuff_alias.size = sizeof(rx_unstuff_mem);
	rx_ppp_unstuff_alias.length = 0;
	buffer_t rx_frame;
	rx_frame.buf = rx_ppp_unstuff_alias.buf;
	rx_frame.size = rx_ppp_unstuff_alias.size;
	rx_frame.len = 0;

	float fpos[NUM_CHANNELS] = {120.f, 10.f, 10.f, 10.f, 10.f, -50.f};
	uint32_t timeout_count = 0;
	while(1)
	{
		for(int ch = 0; ch < NUM_CHANNELS; ch++)
		{
			abh.q_desired[ch] = (int16_t)(fpos[ch] * (32767.f/150.f));
		}
		abh.q_desired[5] = (int16_t)(-55.f * (32767.f/150.f));
		abh_create_movement_frame(&abh, &tx_frame);
		int rc = PPP_stuff(&unstuff_alias, &stuff_buf);

		s.write(stuff_buf.buf, (int)stuff_buf.length);

		int rxlen = s.read_dual_delimiter(rx_buffer_alias.buf, rx_buffer_alias.size, '~', 5);
		if(rxlen >= 0)
		{
			rx_buffer_alias.length = rxlen;	//make sure length gets loaded - serial doesn't natively work with buffer types
			PPP_unstuff(&rx_ppp_unstuff_alias, &rx_buffer_alias);
			// print_ppp_buf(&rx_ppp_unstuff_alias);
			rx_frame.len = rx_ppp_unstuff_alias.length;
			int rc = ahb_parse_movement_reply(&rx_frame, &abh);
			if(rc != ABH_SUCCESS)
			{
				printf("Parsing error! rxlen = %d, %d\n", rc, rx_frame.len);
			}
			printf("%d\n", abh.q[5]);
		}
		else if(rxlen == -2)
		{
			printf("Timeout %d\n", timeout_count++);
		}
		else if(rxlen == 0)
		{
			printf("ERROR: zero length. something is fucked\n");
		}
	}

    return 0;
}
