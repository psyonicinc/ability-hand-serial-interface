#include <iostream>
#include <csignal>
#include <chrono>
#include "serial.h"
#include "PPP.h"
#include "abh_communication.h"

// Global flag for signal handling
volatile sig_atomic_t g_running = 1;

// Signal handler for Ctrl+C
void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		g_running = 0;
	}
}

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

void init_buffer(buffer_t * buf, unsigned char * mem, size_t size)
{
	buf->buf = mem;
	buf->size = size;
	buf->len = 0;
}

void init_ppp_buffer(ppp_buffer_t * buf, unsigned char * mem, size_t size)
{
	buf->buf = mem;
	buf->size = size;
	buf->length = 0;
}


int blocking_ble_radio_setting_change(uint8_t cmd, abh_api_t* abh, buffer_t * tx_frame, ppp_buffer_t * unstuff_alias, ppp_buffer_t * stuff_buf, ppp_buffer_t * rx_buffer_alias, ppp_buffer_t * rx_ppp_unstuff_alias, buffer_t * rx_frame,  Serial& s)
{
	//start bluetooth radio enable/disable
	abh->command_header = cmd;
	abh_create_short_api_frame(abh, tx_frame);
	unstuff_alias->length = tx_frame->len;	//can do just len copy since buf and size are static and initialized before the while loop
	int rc = PPP_stuff(unstuff_alias, stuff_buf);
	s.write(stuff_buf->buf, (int)stuff_buf->length);
	bool ready = false;
	abh->command_header = FIXED_DUMMY_TX1;
	printf("waiting for hand to restart...\n");
	uint32_t num_successful_reads = 0;
	while(ready == false)
	{
		abh_create_short_api_frame(abh, tx_frame);
		unstuff_alias->length = tx_frame->len;	//can do just len copy since buf and size are static and initialized before the while loop
		int rc = PPP_stuff(unstuff_alias, stuff_buf);
		s.write(stuff_buf->buf, (int)stuff_buf->length);
		int rxlen = s.read_dual_delimiter(rx_buffer_alias->buf, rx_buffer_alias->size, '~', 5);
		if(rxlen >= 0)
		{
			rx_buffer_alias->length = rxlen;	//make sure length gets loaded - serial doesn't natively work with buffer types
			PPP_unstuff(rx_ppp_unstuff_alias, rx_buffer_alias);
			// print_ppp_buf(&rx_ppp_unstuff_alias);
			rx_frame->len = rx_ppp_unstuff_alias->length;
			int rc = ahb_parse_movement_reply(rx_frame, abh);
			if(rc != ABH_SUCCESS)
			{
				printf("Parsing error! rxlen = %d, %d\n", rc, rx_frame->len);
			}
			else
			{
				num_successful_reads++;
			}
		}
		if(num_successful_reads > 1000)
		{
			ready = true;
		}
	}
	
	printf("restart complete\n");
	return 0;
}

int main()
{
	// Register signal handler for Ctrl+C
	signal(SIGINT, signal_handler);

	Serial s;
	if(s.autoconnect(921600) == false)
		return -1;
	abh_api_t abh = {};
	abh.address = 0x50;	
	abh.command_header = FIXED_POSITION_CONTROL_TX1;
	
	//initialize tx buffer
	unsigned char tx_mem[128] = {};
	buffer_t tx_frame; 
	tx_frame.buf = tx_mem;
	tx_frame.len = 0;
	tx_frame.size = sizeof(tx_mem);
	
	//initialize raw frame stuffing tx alias for ppp
	ppp_buffer_t unstuff_alias;
	dartt_to_ppp(&tx_frame, &unstuff_alias);

	//initialize ppp stuffed buffer 
	unsigned char stuff_mem[sizeof(tx_mem)*2+2] = {};
	ppp_buffer_t stuff_buf;
	stuff_buf.buf = stuff_mem;
	stuff_buf.length = 0;
	stuff_buf.size = sizeof(stuff_mem);
	
	//initialize recieve buffer and alias
	uint8_t rx_mem[256] = {};
	ppp_buffer_t rx_buffer_alias;	//stuffed
	rx_buffer_alias.buf = rx_mem;
	rx_buffer_alias.length = 0;
	rx_buffer_alias.size = sizeof(rx_mem);

	//initialize rx unstuffing buffer, with both ppp alias and dartt/parser alias
	uint8_t rx_unstuff_mem[128] = {};
	ppp_buffer_t rx_ppp_unstuff_alias;
	rx_ppp_unstuff_alias.buf = rx_unstuff_mem;
	rx_ppp_unstuff_alias.size = sizeof(rx_unstuff_mem);
	rx_ppp_unstuff_alias.length = 0;
	buffer_t rx_frame;
	rx_frame.buf = rx_ppp_unstuff_alias.buf;
	rx_frame.size = rx_ppp_unstuff_alias.size;
	rx_frame.len = 0;


	//floating point targets
	float fpos[NUM_CHANNELS] = {70.f, 10.f, 10.f, 10.f, 10.f, -50.f};
	uint32_t timeout_count = 0;

	uint32_t num_writes = 0;
	uint32_t num_successful_reads = 0;

	// Start timing for frequency calculation


	// blocking_ble_radio_setting_change(ENABLE_BLUETOOTH_RADIO, &abh, &tx_frame, &unstuff_alias, &stuff_buf, &rx_buffer_alias, &rx_ppp_unstuff_alias, &rx_frame, s);
	// blocking_ble_radio_setting_change(DISABLE_BLUETOOTH_RADIO, &abh, &tx_frame, &unstuff_alias, &stuff_buf, &rx_buffer_alias, &rx_ppp_unstuff_alias, &rx_frame, s);
	

	abh.command_header = FIXED_POSITION_CONTROL_TX1;
	auto start_time = std::chrono::steady_clock::now();
	while(g_running)
	{
		const uint8_t tx_command[] = {0x7E, 0x50, 0x10, 0xBB, 0x3B, 0x88, 0x08, 0x88, 0x08, 0x88, 0x08, 0x88, 0x08, 0x12, 0xD1, 0x87, 0x7E};
		s.write((uint8_t*)tx_command, sizeof(tx_command));
		num_writes++;

		//read and parse the response 
		int rxlen = s.read_dual_delimiter(rx_buffer_alias.buf, rx_buffer_alias.size, '~', 5);
		if(rxlen >= 0)
		{
			rx_buffer_alias.length = rxlen;	//make sure length gets loaded - serial doesn't natively work with buffer types
			PPP_unstuff(&rx_ppp_unstuff_alias, &rx_buffer_alias);
			rx_frame.len = rx_ppp_unstuff_alias.length;
			int rc = ahb_parse_movement_reply(&rx_frame, &abh);
			if(rc != ABH_SUCCESS)
			{
				printf("Parsing error! rxlen = %d, %d\n", rc, rx_frame.len);
			}
			else
			{
				num_successful_reads++;
			}
		}
		else if(rxlen == -2)
		{
			printf("Timeout %d\n", timeout_count);
			timeout_count++;
		}
		else if(rxlen == 0)
		{
			printf("ERROR: zero length. \n");
		}
		auto end_time = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
		float elapsed_sec = elapsed_ms / 1000.0f;
		if(elapsed_sec >= 60)
		{
			break;
		}
	}

	// Calculate elapsed time and frequency
	auto end_time = std::chrono::steady_clock::now();
	auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	float elapsed_sec = elapsed_ms / 1000.0f;

	// Print statistics before exit
	printf("\n\n=== Exit Statistics ===\n");
	if (num_writes > 0)
	{
		if (elapsed_sec > 0.0f)
		{
			float avg_frequency = (float)num_successful_reads / elapsed_sec;
			printf("Rate: %.2f Hz\n", avg_frequency);
			printf("Duration: %f sec\n",elapsed_sec);
		}
		printf("Writes: %u\n", num_writes);
		printf("Reads: %u\n", num_successful_reads);

		float success_rate = (1 - ((float)num_successful_reads / (float)num_writes)) * 100.0f;
		printf("Packet Loss: %.2f%%\n", success_rate);

	}
	printf("=======================\n");

	s.disconnect();
    return 0;
}
