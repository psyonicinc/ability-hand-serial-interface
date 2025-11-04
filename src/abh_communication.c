#include "abh_communication.h"

int abh_create_movement_frame(abh_api_t * abh, buffer_t * frame)
{
	if(abh == NULL || frame == NULL || frame->size == 0)
	{
		return -1;
	}

	int bidx = 0;
	frame->buf[bidx++] = abh->address;
	frame->buf[bidx++] = abh->command_header;
	
	return ABH_SUCCESS;
}

/*
Generic 2's complement hex checksum calculation.
Used in the psyonic API
*/
uint8_t abh_get_checksum(uint8_t * arr, int size)
{

	int8_t checksum = 0;
	for (int i = 0; i < size; i++)
		checksum += (int8_t)arr[i];
	return -checksum;
}


/*
 * Load packed 12 bit values located in an 8bit array into
 * an unpacked (zero padded) 16 bit array. FSR utility function
 */
void abh_unpack_8bit_into_12bit(uint8_t* arr, uint16_t* vals, int valsize)
{
	for(int i = 0; i < valsize; i++)
		vals[i] = 0;	//clear the buffer before loading it with |=
	for (int bidx = valsize * 12 - 4; bidx >= 0; bidx -= 4)
	{
		int validx = bidx / 12;
		int arridx = bidx / 8;
		int shift_val = (bidx % 8);
		vals[validx] |= ((arr[arridx] >> shift_val) & 0x0F) << (bidx % 12);
	}
}

/*
Blocking function to parse ability hand frame response/reply
 */
int ahb_parse_movement_reply(buffer_t * frame, abh_api_t * abh)
{
	if(frame == NULL || abh == NULL || frame->len == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	
	uint8_t chksum = abh_get_checksum(frame->buf, frame->len);
	if(chksum != 0)
	{
		return ABH_ERROR_CHECKSUM_MISMATCH;
	}
	int bidx = 0;
	abh->format_header = frame->buf[bidx++];
	uint8_t reply_mode = abh->format_header & 0x0F;
	if(reply_mode < 1 || reply_mode > 3)
	{
		return ABH_ERROR_INVALID_REPLY_MODE;	//placeholder, enumerate explicit error codes later
	}

	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		//extract first value
		uint16_t tmpl = 0;
		tmpl |= (uint16_t)(frame->buf[bidx++]);
		tmpl |= (((uint16_t)(frame->buf[bidx++])) << 8);
		abh->q[ch] = (int16_t)tmpl;

		//extract second value
		tmpl |= (uint16_t)(frame->buf[bidx++]);
		tmpl |= (((uint16_t)(frame->buf[bidx++])) << 8);

		if(reply_mode == 1 || reply_mode == 3)
		{
			abh->iq[ch] = (int16_t)tmpl;
		}
		else if (reply_mode == 2)
		{
			abh->velocity[ch] = (int16_t)tmpl;
		}
	}
	if(reply_mode == 1 || reply_mode == 2)
	{
		abh_unpack_8bit_into_12bit(frame->buf[bidx], abh->fsr_raw, 6);
	}
	else if (reply_mode == 3)
	{
		for(int ch = 0; ch < NUM_CHANNELS; ch++)
		{
			uint16_t tmpl = 0;
			tmpl |= (uint16_t)(frame->buf[bidx++]);
			tmpl |= (((uint16_t)(frame->buf[bidx++])) << 8);
			abh->velocity[ch] = (int16_t)tmpl;
		}
	}
	abh->hot_cold_bitmask = frame->buf[bidx++];

	return ABH_SUCCESS;
}

