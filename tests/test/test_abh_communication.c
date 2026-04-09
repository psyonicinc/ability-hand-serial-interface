#include "abh_communication.h"
#include "unity.h"


void test_abh_create_short_api_frame(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_DUMMY_TX1;  // Read only mode variant 1
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_short_api_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(3, tx.len);  // address(1) + header(1) + checksum(1)

	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(FIXED_DUMMY_TX1, tx.buf[1]);

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_short_api_frame_null_checks(void)
{
	abh_api_t abh;
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	// Test NULL abh
	int rc = abh_create_short_api_frame(NULL, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	// Test NULL frame
	rc = abh_create_short_api_frame(&abh, NULL);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	// Test zero size buffer
	tx.size = 0;
	rc = abh_create_short_api_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
}

void test_abh_create_short_api_frame_buffer_overrun(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_DUMMY_TX2;
	unsigned char tx_mem[2] = {};  // Too small
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_short_api_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);
}

void test_abh_create_movement_frame_position_control(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_POSITION_CONTROL_TX1;

	// Set desired positions for all channels
	abh.q_desired[0] = 100;
	abh.q_desired[1] = 200;
	abh.q_desired[2] = 300;
	abh.q_desired[3] = 400;
	abh.q_desired[4] = 500;
	abh.q_desired[5] = 600;

	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(15, tx.len);  // address(1) + header(1) + 6*int16(12) + checksum(1)

	// Verify header
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(FIXED_POSITION_CONTROL_TX1, tx.buf[1]);

	// Verify first channel position (little endian)
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		int16_t * pi32 = (int16_t*)(&tx.buf[2+ch*2]);
		TEST_ASSERT_EQUAL(abh.q_desired[ch], *pi32);
	}

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_movement_frame_velocity_control(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_VELOCITY_CONTROL_TX2;

	// Set desired velocities
	abh.velocity_desired[0] = -100;
	abh.velocity_desired[1] = -200;
	abh.velocity_desired[2] = -300;
	abh.velocity_desired[3] = -400;
	abh.velocity_desired[4] = -500;
	abh.velocity_desired[5] = -600;

	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(15, tx.len);

	// Verify header
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(FIXED_VELOCITY_CONTROL_TX2, tx.buf[1]);

	// Verify all channel velocities (little endian)
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		int16_t * pi16 = (int16_t*)(&tx.buf[2+ch*2]);
		TEST_ASSERT_EQUAL(abh.velocity_desired[ch], *pi16);
	}

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_movement_frame_torque_control(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_TORQUE_CONTROL_TX3;

	// Set desired currents
	abh.iq_desired[0] = 50;
	abh.iq_desired[1] = 60;
	abh.iq_desired[2] = 70;
	abh.iq_desired[3] = 80;
	abh.iq_desired[4] = 90;
	abh.iq_desired[5] = 100;

	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(15, tx.len);

	// Verify header
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(FIXED_TORQUE_CONTROL_TX3, tx.buf[1]);

	// Verify all channel currents (little endian)
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		int16_t * pi16 = (int16_t*)(&tx.buf[2+ch*2]);
		TEST_ASSERT_EQUAL(abh.iq_desired[ch], *pi16);
	}

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_movement_frame_voltage_control(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_VOLTAGE_CONTROL_TX1;

	// Set desired voltages
	abh.vq_desired[0] = 1000;
	abh.vq_desired[1] = 2000;
	abh.vq_desired[2] = 3000;
	abh.vq_desired[3] = 3546;  // Max voltage from spec
	abh.vq_desired[4] = -3546;
	abh.vq_desired[5] = 0;

	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(15, tx.len);

	// Verify header
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(FIXED_VOLTAGE_CONTROL_TX1, tx.buf[1]);

	// Verify all channel voltages (little endian)
	for(int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		int16_t * pi16 = (int16_t*)(&tx.buf[2+ch*2]);
		TEST_ASSERT_EQUAL(abh.vq_desired[ch], *pi16);
	}

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_movement_frame_invalid_header(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = 0xFF;  // Invalid header

	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_HEADER, rc);
}

void test_abh_create_movement_frame_null_checks(void)
{
	abh_api_t abh;
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	// Test NULL abh
	int rc = abh_create_movement_frame(NULL, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	// Test NULL frame
	rc = abh_create_movement_frame(&abh, NULL);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
}

void test_abh_create_movement_frame_buffer_overrun(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	abh.command_header = FIXED_POSITION_CONTROL_TX1;
	unsigned char tx_mem[10] = {};  // Too small
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_movement_frame(&abh, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);
}

void test_movement_reply_basic_fuzzing(void)
{
	int rc = abh_parse_movement_reply(NULL, NULL);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	abh_api_t abh = {};
	unsigned char rxmem[128] = {};
	rc = abh_parse_movement_reply(NULL, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	abh_buffer_t rx = {.buf = NULL, .size = 10, .len = 5};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	rx = (abh_buffer_t){.buf = rxmem, .size = 0, .len = 5};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);

	rx = (abh_buffer_t){.buf = rxmem, .size = sizeof(rxmem), .len = sizeof(rxmem) + 1};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);

	rx = (abh_buffer_t){.buf = rxmem, .size = sizeof(rxmem), .len = 0};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	rx = (abh_buffer_t){.buf = rxmem, .size = sizeof(rxmem), .len = 1};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_CHECKSUM_MISMATCH, rc);

	rx = (abh_buffer_t){.buf = rxmem, .size = sizeof(rxmem), .len = sizeof(rxmem)};
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);	//succeeds from matching checksum

	rx.len = 0;
	for(size_t i = 0; i < 11; i++)
	{
		rx.buf[rx.len++] = i+1;
	}
	rx.buf[rx.len] = abh_get_checksum(rx.buf, rx.len) - 1;
	rx.len++;
	rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_CHECKSUM_MISMATCH, rc);	//succeeds from matching checksum
	
}

void test_movement_reply_invalid_reply_mode(void)
{
	abh_api_t abh = {};
	abh.address = 0x50;
	unsigned char rxmem[128] = {};
	abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};
	rx.buf[rx.len++] = 0x3;		//invalid
	rx.buf[rx.len++] = abh_get_checksum(rx.buf, rx.len);
	int rc = abh_parse_movement_reply(&rx, &abh);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_REPLY_MODE, rc);
	
}


