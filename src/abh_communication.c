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
	
	return 0;
}


/*
Blocking function to parse ability hand frame response/reply
 */
int ahb_parse_movement_reply(buffer_t * frame, abh_api_t * abh)
{
	if(frame == NULL || abh == NULL || frame->len == 0)
	{
		return -1;
	}

	abh->format_header = frame->buf[0];

	return 0;
}
