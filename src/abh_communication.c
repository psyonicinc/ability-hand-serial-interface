#include "abh_communication.h"



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

/**
 * Helper function to append a uint32_t to the end of a buffer_t, little endian
 */
int append_uint32(uint32_t value, buffer_t * frame)
{
	if(frame == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < frame->len + sizeof(uint32_t))
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	frame->buf[frame->len++] = (unsigned char)(value & 0x000000FF);
	frame->buf[frame->len++] = (unsigned char)((value & 0x0000FF00) >> 8);
	frame->buf[frame->len++] = (unsigned char)((value & 0x00FF0000) >> 16);
	frame->buf[frame->len++] = (unsigned char)((value & 0xFF000000) >> 24);
	return ABH_SUCCESS;
}

/**
 * Helper function to append a uint16_t to the end of a buffer_t, little endian
 */
int append_uint16(uint16_t value, buffer_t * frame)
{
	if(frame == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < frame->len + sizeof(uint16_t))
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	frame->buf[frame->len++] = (unsigned char)(value & 0x00FF);
	frame->buf[frame->len++] = (unsigned char)((value & 0xFF00) >> 8);
	return ABH_SUCCESS;
}

/*
* Create a 'retister write' frame with register address and value arguments
*/
int abh_create_write_register_frame(abh_api_t * abh, uint32_t reg_address, uint32_t value, buffer_t * frame)
{
	if(abh == NULL || frame == NULL || frame->size == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < 11)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	abh->command_header = UART_WRITE_REGISTER;	//rather than throw an error, just assume user knows what they want - load the header
	frame->len = 0;
	frame->buf[frame->len++] = abh->address;
	frame->buf[frame->len++] = abh->command_header;	
	append_uint32(reg_address, frame);	//overrun checked at start, so no need to handle rc
	append_uint32(value, frame);
	frame->buf[frame->len] = abh_get_checksum((uint8_t*)frame->buf, (int)frame->len);
	frame->len++;
	return ABH_SUCCESS;
}

/*
Read register request frame
*/
int abh_create_read_register_frame(abh_api_t  * abh, uint32_t reg_address, buffer_t * frame)
{
	if(abh == NULL || frame == NULL || frame->size == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < 7)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	abh->command_header = UART_READ_REGISTER;	//rather than throw an error, just assume user knows what they want - load the header
	frame->len = 0;
	frame->buf[frame->len++] = abh->address;
	frame->buf[frame->len++] = abh->command_header;	
	append_uint32(reg_address, frame);	//overrun checked at start, so no need to handle rc
	frame->buf[frame->len] = abh_get_checksum((uint8_t*)frame->buf, (int)frame->len);
	frame->len++;
	return ABH_SUCCESS;
}

/*
Loads pass-by-reference result of read reply frame with the valid result, or returns with an error
*/
int abh_parse_read_register_reply(buffer_t * frame, abh_api_t * abh, uint32_t * reply_word)
{
	return 0;
}

/*
Parse the response issued from a write register request
*/
int abh_parse_write_register_reply(buffer_t * frame, abh_api_t * abh, uint32_t * reply_word)
{
	return 0;
}

/*
 * For 3 byte frames - like read only, etc. 
 */
int abh_create_short_api_frame(abh_api_t * abh, buffer_t * frame)
{
	if(abh == NULL || frame == NULL || frame->size == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < 3)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	//TODO: add some validation of command header value - it can only be a certain range of values
	frame->len = 0;
	frame->buf[frame->len++] = abh->address;
	frame->buf[frame->len++] = abh->command_header;
	frame->buf[frame->len] = abh_get_checksum((uint8_t*)frame->buf, (int)frame->len);
	frame->len++;
	return ABH_SUCCESS;
}

/*
 *
 * Create a pre-encoding/framing ability hand control frame
 * 
 */
int abh_create_movement_frame(abh_api_t * abh, buffer_t * frame)
{
	if(abh == NULL || frame == NULL || frame->size == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	if(frame->size < 15)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}

	frame->len = 0;
	frame->buf[frame->len++] = abh->address;
	frame->buf[frame->len++] = abh->command_header;
	uint16_t tmpr = 0;
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		switch(abh->command_header)
		{
			case FIXED_POSITION_CONTROL_TX1: 
			case FIXED_POSITION_CONTROL_TX2: 
			case FIXED_POSITION_CONTROL_TX3: 
			{	
				tmpr = (uint16_t)(abh->q_desired[ch]);
				break;
			}
			case FIXED_VELOCITY_CONTROL_TX1: 
			case FIXED_VELOCITY_CONTROL_TX2: 
			case FIXED_VELOCITY_CONTROL_TX3: 
			{
				tmpr = (uint16_t)(abh->velocity_desired[ch]);
				break;
			}
			case FIXED_TORQUE_CONTROL_TX1: 
			case FIXED_TORQUE_CONTROL_TX2: 
			case FIXED_TORQUE_CONTROL_TX3: 
			{
				tmpr = (uint16_t)(abh->iq_desired[ch]);
				break;
			}
			case FIXED_VOLTAGE_CONTROL_TX1: 
			case FIXED_VOLTAGE_CONTROL_TX2: 
			case FIXED_VOLTAGE_CONTROL_TX3: 
			{
				tmpr = (uint16_t)(abh->vq_desired[ch]);
				break;
			}
			default:
			{
				return ABH_ERROR_INVALID_HEADER;
			}
		};
		append_uint16(tmpr, frame);	//top buffer overrun catches overrun scenario, so no need to check return value
	}
	frame->buf[frame->len] = abh_get_checksum((uint8_t*)frame->buf, (int)frame->len);
	frame->len++;
	return ABH_SUCCESS;
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
	
	uint8_t chksum = abh_get_checksum((uint8_t*)frame->buf, (int)frame->len);
	if(chksum != 0)
	{
		return ABH_ERROR_CHECKSUM_MISMATCH;
	}
	int bidx = 0;
	abh->format_header = frame->buf[bidx++];
	uint8_t reply_mode = (abh->format_header & 0x0F) + 1;
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
		tmpl = (uint16_t)(frame->buf[bidx++]);
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
		abh_unpack_8bit_into_12bit(&frame->buf[bidx], abh->fsr_raw, NUM_FINGERS*NUM_FSR_PER_FINGER);
		bidx += 45;	//total bytes
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

