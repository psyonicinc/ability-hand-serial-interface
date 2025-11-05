#include "abh_communication.h"
#include "unity.h"

void test_abh_create_read_register_frame(void)
{
	abh_api_t abh;
	abh.address = 0x50;
	unsigned char tx_mem[128] = {};
	buffer_t tx = {.buf = tx_mem, .len = 0, .size = sizeof(tx_mem)};
	int rc = abh_create_read_register_frame(&abh, 0x12, &tx);
	TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
	TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
	TEST_ASSERT_EQUAL(UART_READ_REGISTER, tx.buf[1]);
	uint32_t * pu32 = (uint32_t*)(&tx.buf[2]);
	TEST_ASSERT_EQUAL(0x12, (uint32_t)(*pu32));
}
