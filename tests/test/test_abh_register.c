#include "abh_communication.h"
#include "unity.h"


void test_abh_create_read_register_frame(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};
	int rc = abh_create_read_register_frame(&abh, 0x12, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(UART_READ_REGISTER, tx.buf[1]);
	uint32_t * pu32 = (uint32_t*)(&tx.buf[2]);
	TEST_ASSERT_EQUAL(0x12, (uint32_t)(*pu32));
}


void test_abh_create_write_register_frame(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	// Test basic write register frame creation
	int rc = abh_create_write_register_frame(&abh, 0xDEADBEEF, 0x12345678, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(11, tx.len);  // address(1) + header(1) + reg_addr(4) + value(4) + checksum(1)

	// Verify frame structure
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);  // address
	TEST_ASSERT_EQUAL(UART_WRITE_REGISTER, tx.buf[1]);  // command header

	// Verify register address (little endian)
	uint32_t *reg_addr = (uint32_t*)(&tx.buf[2]);
	TEST_ASSERT_EQUAL(0xDEADBEEF, *reg_addr);

	// Verify value (little endian)
	uint32_t *value = (uint32_t*)(&tx.buf[6]);
	TEST_ASSERT_EQUAL(0x12345678, *value);

	// Verify checksum
	uint8_t chk = abh_get_checksum(tx.buf, tx.len);
	TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_write_register_frame_null_checks(void)
{
	abh_api_t abh;
	unsigned char tx_mem[128] = {};
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	// Test NULL abh pointer
	int rc = abh_create_write_register_frame(NULL, 0, 0, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	// Test NULL frame pointer
	rc = abh_create_write_register_frame(&abh, 0, 0, NULL);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

	// Test zero size buffer
	tx.size = 0;
	rc = abh_create_write_register_frame(&abh, 0, 0, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
}

void test_abh_create_write_register_frame_buffer_overrun(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	unsigned char tx_mem[5] = {};  // Too small
	abh_buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};

	int rc = abh_create_write_register_frame(&abh, 0, 0, &tx);
	TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);
}
