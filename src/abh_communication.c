#include "abh_communication.h"

/*length of the mandatory motor section in reply*/
static const size_t nbytes_motor_section = 24;		//pre-check length
/*length of the optional fsr section for reply 1 and 2 types*/
static const size_t nbytes_packed_fsr_data = 45;
/*length of the optional motor section for reply 3 types*/
static const size_t nbytes_tail_motor_section = 12;

/*Helper for checking abh buffers. DRY*/
int check_msg(abh_buffer_t* msg);

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
	Helper to verify checksum validity
*/
bool abh_verify_checksum(abh_buffer_t * msg)
{
	if(check_msg(msg) != ABH_SUCCESS)
	{
		return false;
	}
	if(msg->len <= sizeof(uint8_t))//reply must be at least 2 bytes for a 1-byte checksum signature to be valid
	{
		return false;
	}
	uint16_t chksum = abh_get_checksum(msg->buf, msg->len - sizeof(uint8_t));
    unsigned char m_chksum = msg->buf[(msg->len - sizeof(uint8_t))];
	if(chksum == m_chksum)
	{
		// msg->len = msg->len - sizeof(uint8_t);	//remove checksum
		return true;
	}
	else
	{
		return false;
	}
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
 * Helper function to append a uint32_t to the end of a abh_buffer_t, little endian
 */
int append_uint32(uint32_t value, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
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
 * Helper function to append a uint16_t to the end of a abh_buffer_t, little endian
 */
int append_uint16(uint16_t value, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
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
int abh_create_write_register_frame(abh_api_t * abh, uint32_t reg_address, uint32_t value, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}
	if(abh == NULL)
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
int abh_create_read_register_frame(abh_api_t  * abh, uint32_t reg_address, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}

	if(abh == NULL)
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

TODO: DEPRECATE THIS INTERFACE. REPLACE WITH DARTT.
DO NOT MAP TO MODBUS.
DO MAP DARTT TO MODBUS.

Register replies have the following possible responses:
If a write request was generated:
	1. 0x4E11FEA4 (success)
	2. 0xFA17FFFF (fail)
If a read request was generated:
	1. [0x4E11FEA4][value][checksum]
*/
int abh_parse_register_reply(abh_buffer_t * frame, abh_api_t * abh, abh_register_reply_t * msg)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}
	if(abh == NULL || msg == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	//TODO: add more input validity checks. Reduce repeated patterns for this with helper functions

	/*Obtain the header value. Code out if the number of bytes recieved is smaller than a valid header*/
	uint32_t header = 0;
	size_t msg_pos = 0;
	if(frame->len >= sizeof(uint32_t))
	{
		for(size_t i = 0; i < sizeof(uint32_t); i++)
		{
			header |= ((uint32_t)frame->buf[msg_pos]) << (i*8);
			msg_pos++;
		}
	}
	else	//invalid due to small size, code out
	{
		msg->request_valid = false;
		msg->reply_type = ABH_REGISTER_INVALID_REPLY;
		return ABH_ERROR_INVALID_REGISTER_REPLY;
	}

	/*parse header into request_valid variable*/
	if(header == ABH_REGISTER_SUCCESS)
	{
		msg->request_valid = true;
	}
	else if (header == ABH_REGISTER_FAIL)
	{
		msg->request_valid = false;
	}
	else	//invalid due to invalid header, code out
	{
		msg->request_valid = false;
		msg->reply_type = ABH_REGISTER_INVALID_REPLY;
		return ABH_ERROR_INVALID_REGISTER_REPLY;	
	}


	if(frame->len == ABH_REGISTER_WRITE_REPLY_LEN)	
	{
		msg->reply_type = ABH_REGISTER_WRIE_REPLY;	//if we're here, it's a valid header of size 4. So we know it's a write reply and we know that we should return after loading that.
		return ABH_SUCCESS;
	}
	else if (frame->len != ABH_REGISTER_READ_REPLY_LEN)		//if we're here, the length must be equal to read reply length or it's invalid
	{
		msg->request_valid = false;
		msg->reply_type = ABH_REGISTER_INVALID_REPLY;
		return ABH_ERROR_INVALID_REGISTER_REPLY;
	}

	bool valid = abh_verify_checksum(frame);
	if(valid == false)
	{
		msg->request_valid = false;
		msg->reply_type = ABH_REGISTER_INVALID_REPLY;
		return ABH_ERROR_CHECKSUM_MISMATCH;
	}

	//by making it here, we know the length precisely matches 
	msg->request_valid = true;
	msg->reply_type = ABH_REGISTER_READ_REPLY;
	msg->register_value = 0;
	for(size_t i = 0; i < sizeof(uint32_t); i++)
	{
		msg->register_value |= ((uint32_t)frame->buf[msg_pos]) << (i*8);
		msg_pos++;
	}
	return ABH_SUCCESS;
}

/*
 * For 3 byte frames - like read only, etc. 
 */
int abh_create_short_api_frame(abh_api_t * abh, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}
	if(abh == NULL)
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
int abh_create_movement_frame(abh_api_t * abh, abh_buffer_t * frame)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}

	if(abh == NULL)
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
int abh_parse_movement_reply(abh_buffer_t * frame, abh_api_t * abh)
{
	int chkrc = check_msg(frame);
	if(chkrc != ABH_SUCCESS)
	{
		return chkrc;
	}

	if(abh == NULL || frame->len == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	
	bool checksum_valid = abh_verify_checksum(frame);
	if(checksum_valid == false)
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

	//pre-check overrun before diving into the frame parser
	if(frame->len < bidx + nbytes_motor_section)	
	{
		return ABH_ERROR_REPLY_LEN_INVALID;
	}
	//extract motor section values
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
	//grab fsr data
	if(reply_mode == 1 || reply_mode == 2)
	{
		if(frame->len < bidx + nbytes_packed_fsr_data)
		{
			return ABH_ERROR_REPLY_LEN_INVALID;
		}
		abh_unpack_8bit_into_12bit(&frame->buf[bidx], abh->fsr_raw, NUM_FINGERS*NUM_FSR_PER_FINGER);
		bidx += nbytes_packed_fsr_data;	//total bytes
	}
	else if (reply_mode == 3)
	{
		if(frame->len < bidx + nbytes_tail_motor_section)	//pre-check overrun before pulling out velocity in type 3
		{
			return ABH_ERROR_REPLY_LEN_INVALID;
		}
		for(int ch = 0; ch < NUM_CHANNELS; ch++)
		{
			uint16_t tmpl = 0;
			tmpl |= (uint16_t)(frame->buf[bidx++]);
			tmpl |= (((uint16_t)(frame->buf[bidx++])) << 8);
			abh->velocity[ch] = (int16_t)tmpl;
		}
	}
	if(frame->len < bidx + 1)	
	{
		return ABH_ERROR_REPLY_LEN_INVALID;
	}
	abh->hot_cold_bitmask = frame->buf[bidx++];
	if(frame->len != bidx + 1)	//checksum is at position bidx, len should be bidx+1
	{
		return ABH_ERROR_REPLY_LEN_INVALID;
	}
	return ABH_SUCCESS;
}

/*
	Helper for checking abh buffers
*/
int check_msg(abh_buffer_t* msg)
{
	if(msg == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	else if(msg->buf == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	else if(msg->len > msg->size)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}
	else if(msg->size == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	return ABH_SUCCESS;
}
