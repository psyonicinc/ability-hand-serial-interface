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
	int16_t q[NUM_CHANNELS];
	int16_t qd[NUM_CHANNELS];
	int16_t vq_d[NUM_CHANNELS];
	int16_t iq_d[NUM_CHANNELS];
	int16_t fsr_raw[NUM_FINGERS*NUM_FSR_PER_FINGER];
}abh_api_t;

