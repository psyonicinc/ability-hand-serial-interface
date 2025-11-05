#ifndef ABH_COMMUNICATION_H
#define ABH_COMMUNICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "dartt.h"

#define NUM_CHANNELS 6
#define NUM_FSR_PER_FINGER 6
#define NUM_FINGERS	5

enum 
{
	FIXED_DUMMY_TX1 = 0xA0,	//read only, reply mode 1
    FIXED_DUMMY_TX2 = 0xA1, //read only, reply mode 2
    FIXED_DUMMY_TX3 = 0xA2, //read only, reply mode 3
	FIXED_POSITION_CONTROL_TX1 = 0x10,
    FIXED_POSITION_CONTROL_TX2 = 0x11, 
    FIXED_POSITION_CONTROL_TX3 = 0x12,
    FIXED_VELOCITY_CONTROL_TX1 = 0x20,
    FIXED_VELOCITY_CONTROL_TX2 = 0x21,
    FIXED_VELOCITY_CONTROL_TX3 = 0x22,
    FIXED_TORQUE_CONTROL_TX1 = 0x30,
    FIXED_TORQUE_CONTROL_TX2 = 0x31,
    FIXED_TORQUE_CONTROL_TX3 = 0x32, 
    FIXED_VOLTAGE_CONTROL_TX1 = 0x40, 
    FIXED_VOLTAGE_CONTROL_TX2 = 0x41,
    FIXED_VOLTAGE_CONTROL_TX3 = 0x42,

	UART_WRITE_REGISTER = 0xDE,	//command header for write setting
	UART_READ_REGISTER = 0xDA,	//command header for read setting
	ENABLE_BLUETOOTH_RADIO = 0x07,	//command to enable bluetooth radio
	DISABLE_BLUETOOTH_RADIO = 0x08,
	RESTART_HAND = 0x09,
	API_EXIT_CMD = 0x7C
};

enum
{
	ABH_SUCCESS = 0,
	ABH_ERROR_INVALID_ARGUMENT = -1,
	ABH_ERROR_CHECKSUM_MISMATCH = -2,
	ABH_ERROR_INVALID_REPLY_MODE = -3,
	ABH_ERROR_INVALID_HEADER = -4,
	ABH_ERROR_BUFFER_OVERRUN = -5,
	ABH_ERROR_NOT_CONNECTED = -6,
	ABH_ERROR_TIMEOUT = -7,
	ABH_ERROR_CONNECTION_FAILED = -8,
	ABH_ERROR_ALREADY_CONNECTED = -9,
	ABH_ERROR_WRITE_FAILED = -10
};

/*
	Typedef struct wrapper for ability hand API data.
	Contains all api-related parameters.
	TODO - eventually set up as a dartt parameter list, potentially with dartt generated headers
*/
typedef struct abh_api_t
{
	uint8_t address;
	uint8_t command_header;	//frame type, i.e. FIXED_POSITION_CONTROL_TX1...
	uint8_t format_header;	//reply header
	int16_t q[NUM_CHANNELS];	//actual position
	int16_t q_desired[NUM_CHANNELS];	//desired position
	int16_t vq_desired[NUM_CHANNELS];	//desired voltage
	int16_t iq[NUM_CHANNELS];	//actual current
	int16_t iq_desired[NUM_CHANNELS];	//desired current
	int16_t velocity[NUM_CHANNELS];	//actual velocity
	int16_t velocity_desired[NUM_CHANNELS];
	int16_t fsr_raw[NUM_FINGERS*NUM_FSR_PER_FINGER];
	uint8_t hot_cold_bitmask;

}abh_api_t;

uint8_t abh_get_checksum(uint8_t * arr, int size);
int abh_create_write_register_frame(abh_api_t * abh, uint32_t reg_address, uint32_t value, buffer_t * frame);
int abh_create_read_register_frame(abh_api_t  * abh, uint32_t reg_address, buffer_t * frame);
int abh_parse_read_register_reply(buffer_t * frame, abh_api_t * abh, uint32_t * reply_word);
int abh_create_short_api_frame(abh_api_t * abh, buffer_t * frame);
int abh_create_movement_frame(abh_api_t * abh, buffer_t * frame);
int ahb_parse_movement_reply(buffer_t * frame, abh_api_t * abh);

#ifdef __cplusplus
}
#endif

#endif
