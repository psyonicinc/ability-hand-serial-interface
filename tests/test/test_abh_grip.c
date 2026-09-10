#include <string.h>
#include "abh_communication.h"
#include "unity.h"

#define GRIP_REPLY_LEN 11 // 8 data bytes + 1 demo byte + 1 checksum

void helper_build_grip_reply(abh_buffer_t * frame, uint16_t vibration, uint8_t handshake_hi, uint8_t handshake_lo, uint8_t r, uint8_t g, uint8_t b, uint8_t status, bool idle, uint8_t percent)
{
    frame->len = 0;
    
    frame->buf[frame->len++] = (unsigned char)(vibration & 0x00FF);
    frame->buf[frame->len++] = (unsigned char)((vibration & 0xFF00) >> 8);

    frame->buf[frame->len++] = handshake_hi;
    frame->buf[frame->len++] = handshake_lo;

    frame->buf[frame->len++] = r;
    frame->buf[frame->len++] = g;
    frame->buf[frame->len++] = b;

    frame->buf[frame->len++] = status;

    frame->buf[frame->len++] = (uint8_t)((idle ? 0x80 : 0x00) | (percent & 0x7F));
    frame->buf[frame->len++] = 0xFE; // demo byte, unused by parser
    frame->buf[frame->len++] = abh_get_checksum(frame->buf, frame->len);
}

void test_abh_parse_grip_reply_null_checks(void)
{
    int rc = abh_parse_grip_reply(NULL, NULL);
    TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    rc = abh_parse_grip_reply(NULL, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

    rc = abh_parse_grip_reply(&rx, NULL);
    TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
}

void test_abh_grip_reply_buffer_overrun(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};

    abh_buffer_t rx = {.buf = NULL, .size = sizeof(rxmem), .len = 5};
    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

    rx = (abh_buffer_t){.buf = rxmem, .size = sizeof(rxmem), .len = sizeof(rxmem) + 1};
    rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);

    rx = (abh_buffer_t){.buf = rxmem, .size = 0, .len = 0};
    rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
}

void test_abh_parse_grip_reply_invalid_length(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    helper_build_grip_reply(&rx, 100, 0xBE, 0xEF, 1, 2, 3, 4, false, 50);
    TEST_ASSERT_EQUAL(GRIP_REPLY_LEN, rx.len);

    rx.len = GRIP_REPLY_LEN - 1;
    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_REPLY_LEN_INVALID, rc);

    rx.len = GRIP_REPLY_LEN + 1;
    rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_REPLY_LEN_INVALID, rc);
}

void test_abh_parse_grip_reply_checksum_mismatch(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    helper_build_grip_reply(&rx, 100, 0xBE, 0xEF, 1, 2, 3, 4, false, 50);
    rx.buf[rx.len - 1] ^= 0xFF; //corrupt the checksum byte
    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_ERROR_CHECKSUM_MISMATCH, rc);
}

void test_abh_parse_grip_reply_success_fields(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    helper_build_grip_reply(&rx, 0x1234, 0xBE, 0xEF, 0x10, 0x20, 0x30, 0x05, true, 0x64);

    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);

    TEST_ASSERT_EQUAL(0x1234, grip.vibration_motor_intensity);
    TEST_ASSERT_TRUE(grip.handshake_valid);
    TEST_ASSERT_EQUAL(0x10, grip.grip_color[0]);
    TEST_ASSERT_EQUAL(0x20, grip.grip_color[1]);
    TEST_ASSERT_EQUAL(0x30, grip.grip_color[2]);
    TEST_ASSERT_EQUAL(0x05, grip.grip_status);
    TEST_ASSERT_TRUE(grip.idle);
    TEST_ASSERT_EQUAL(0x64, grip.grip_percent);
}

void test_abh_parse_grip_reply_handshake_invalid(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    helper_build_grip_reply(&rx, 0, 0xEF, 0xBE, 0, 0, 0, 0, false, 0);

    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
    TEST_ASSERT_FALSE(grip.handshake_valid);
}

void test_abh_parse_grip_reply_idle_percent_bit_split(void)
{
    abh_grip_t grip = {};
    unsigned char rxmem[128] = {};
    abh_buffer_t rx = {.buf = rxmem, .size = sizeof(rxmem), .len = 0};

    helper_build_grip_reply(&rx, 0, 0xBE, 0xEF, 0, 0, 0, 0, false, 0x7F);
    int rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
    TEST_ASSERT_FALSE(grip.idle);
    TEST_ASSERT_EQUAL(0x7F, grip.grip_percent);

    helper_build_grip_reply(&rx, 0, 0xBE, 0xEF, 0, 0, 0, 0, true, 0x7F);
    rc = abh_parse_grip_reply(&rx, &grip);
    TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
    TEST_ASSERT_TRUE(grip.idle);
    TEST_ASSERT_EQUAL(0x7F, grip.grip_percent);
}

void test_abh_create_grip_frame(void)
{
    abh_grip_t grip = {};
    grip.address = 0x50;
    unsigned char txmem[128] = {};
    abh_buffer_t tx = {.buf = txmem, .size = sizeof(txmem), .len = 0};

    int rc = abh_create_grip_frame(&grip, 0x06, 0xFF, &tx);
    TEST_ASSERT_EQUAL(ABH_SUCCESS, rc);
    TEST_ASSERT_EQUAL(5, tx.len);

    TEST_ASSERT_EQUAL(0x50, tx.buf[0]);
    TEST_ASSERT_EQUAL(GRIP_CONTROL, tx.buf[1]);
    TEST_ASSERT_EQUAL(0x06, tx.buf[2]);
    TEST_ASSERT_EQUAL(0xFF, tx.buf[3]);

    uint8_t chk = abh_get_checksum(tx.buf, tx.len);
    TEST_ASSERT_EQUAL(0, chk);
}

void test_abh_create_grip_frame_null_checks(void)
  {
      abh_grip_t grip = {};
      unsigned char txmem[128] = {};
      abh_buffer_t tx = {.buf = txmem, .size = sizeof(txmem), .len = 0};

      int rc = abh_create_grip_frame(NULL, 0x00, 0x00, &tx);
      TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

      rc = abh_create_grip_frame(&grip, 0x00, 0x00, NULL);
      TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);

      tx.size = 0;
      rc = abh_create_grip_frame(&grip, 0x00, 0x00, &tx);
      TEST_ASSERT_EQUAL(ABH_ERROR_INVALID_ARGUMENT, rc);
  }

  void test_abh_create_grip_frame_buffer_overrun(void)
  {
      abh_grip_t grip = {};
      grip.address = 0x50;
      unsigned char txmem[4] = {};  // Too small (frame needs 5 bytes)
      abh_buffer_t tx = {.buf = txmem, .size = sizeof(txmem), .len = 0};

      int rc = abh_create_grip_frame(&grip, 0x01, 0x80, &tx);
      TEST_ASSERT_EQUAL(ABH_ERROR_BUFFER_OVERRUN, rc);
  }
