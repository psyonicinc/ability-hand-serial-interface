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

void helper_append_uint32(uint32_t value, abh_buffer_t * frame)
{
	frame->buf[frame->len++] = (unsigned char)(value & 0x000000FF);
	frame->buf[frame->len++] = (unsigned char)((value & 0x0000FF00) >> 8);
	frame->buf[frame->len++] = (unsigned char)((value & 0x00FF0000) >> 16);
	frame->buf[frame->len++] = (unsigned char)((value & 0xFF000000) >> 24);
}

uint32_t helper_extract_uint32(unsigned char * start)
{
	uint32_t val = 0;
	for(size_t i = 0; i < sizeof(uint32_t); i++)
	{
		uint32_t cast = (uint32_t)(start[i]);
		val |=  (cast << (i*8));
	}
	return val;
}

void test_uint32_helpers(void)
{
	unsigned char rx_mem[32] = {};
	abh_buffer_t rx = {.buf = rx_mem, .size = sizeof(rx_mem), .len = 0};
	helper_append_uint32(0xABCDEF76, &rx);
	helper_append_uint32(0xDEADBEEF, &rx);
	TEST_ASSERT_EQUAL(8, rx.len);
	uint32_t v1 = helper_extract_uint32(rx.buf);
	uint32_t v2 = helper_extract_uint32(rx.buf + sizeof(uint32_t));
	TEST_ASSERT_EQUAL(0xABCDEF76, v1);
	TEST_ASSERT_EQUAL(0xDEADBEEF, v2);
}

void test_register_invalid_reply(void)
{
	abh_register_reply_t msg = {};
	unsigned char rx_mem[32] = {};
	abh_buffer_t rx = {.buf = rx_mem, .size = sizeof(rx_mem), .len = 0};
	rx.len = 0;
	int rc = 0;
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(false, msg.register_value);
	TEST_ASSERT_EQUAL(ABH_REGISTER_INVALID_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_REGISTER_REPLY, rc);

	rx.len = 3;
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(false, msg.register_value);
	TEST_ASSERT_EQUAL(ABH_REGISTER_INVALID_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_REGISTER_REPLY, rc);

	rx.len = 5;
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(false, msg.register_value);
	TEST_ASSERT_EQUAL(ABH_REGISTER_INVALID_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_REGISTER_REPLY, rc);

	rx.len = 0;
	helper_append_uint32(0xDEADBEEF, &rx);
	TEST_ASSERT_EQUAL(sizeof(uint32_t), rx.len);
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(false, msg.register_value);
	TEST_ASSERT_EQUAL(ABH_REGISTER_INVALID_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_REGISTER_REPLY, rc);

}

void test_register_valid_write_reply(void)
{
	abh_register_reply_t msg = {.register_value = 0xABCDEF77, .reply_type = ABH_REGISTER_INVALID_REPLY, .request_valid = false};
	unsigned char rx_mem[32] = {};
	abh_buffer_t rx = {.buf = rx_mem, .size = sizeof(rx_mem), .len = 0};
	int rc = 0;

	rx.len = 0;
	helper_append_uint32(ABH_REGISTER_SUCCESS, &rx);
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(true, msg.request_valid);
	TEST_ASSERT_EQUAL(ABH_REGISTER_WRITE_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(0xABCDEF77, msg.register_value);

	msg = (abh_register_reply_t){.register_value = 0xABCDEF77, .reply_type = ABH_REGISTER_INVALID_REPLY, .request_valid = false};
	rx.len = 0;
	helper_append_uint32(ABH_REGISTER_FAIL, &rx);
	TEST_ASSERT_EQUAL(4, rx.len);
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(false, msg.request_valid);
	TEST_ASSERT_EQUAL(ABH_REGISTER_WRITE_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(0xABCDEF77, msg.register_value);

}

void test_register_valid_read_reply(void)
{
	abh_register_reply_t msg = {.register_value = 0xABCDEF77, .reply_type = ABH_REGISTER_INVALID_REPLY, .request_valid = false};
	unsigned char rx_mem[32] = {};
	abh_buffer_t rx = {.buf = rx_mem, .size = sizeof(rx_mem), .len = 0};
	int rc = 0;

	helper_append_uint32(ABH_REGISTER_SUCCESS, &rx);
	helper_append_uint32(0x12345678, &rx);
	rx.buf[rx.len++] = abh_get_checksum(rx.buf, rx.len);
	rc = abh_parse_register_reply(&rx, &msg);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(true, msg.request_valid);
	TEST_ASSERT_EQUAL(ABH_REGISTER_READ_REPLY, msg.reply_type);
	TEST_ASSERT_EQUAL(0x12345678, msg.register_value);
}
