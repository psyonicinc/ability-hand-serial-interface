#include <stdint.h>
#include "dartt.h"

#define NUM_CHANNELS 6
#define NUM_FSR_PER_FINGER 6
#define NUM_FINGERS	5

/*
	Typedef struct wrapper for ability hand API data.
	Contains all api-related parameters.
	TODO - eventually set up as a dartt parameter list, potentially with dartt generated headers
*/
typedef struct abh_api_t
{
	uint8_t address;
	int16_t q[NUM_CHANNELS];
	int16_t qd[NUM_CHANNELS];
	int16_t vq_d[NUM_CHANNELS];
	int16_t iq_d[NUM_CHANNELS];
	int16_t fsr_raw[NUM_FINGERS*NUM_FSR_PER_FINGER];
}abh_api_t;

