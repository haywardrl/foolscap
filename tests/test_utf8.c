#include "app/util/utf8.h"

#include <stdint.h>
#include <unity.h>

void setUp(void) {
}

void tearDown(void) {
}

static void test_ascii_caps_return_ok(void) {
    utf8_iter_t it;
    const char *input = "A";
    utf8_iter_init(&it, input, 1);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x41, cp);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_ascii_lowercase_return_ok(void) {
    utf8_iter_t it;
    const char *input = "z";
    utf8_iter_init(&it, input, 1);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x7A, cp);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_ascii_digits_return_ok(void) {
    utf8_iter_t it;
    const char *input = "1";
    utf8_iter_init(&it, input, 1);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x31, cp);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_ascii_newline_return_ok(void) {
    utf8_iter_t it;
    const char *input = "\n";
    utf8_iter_init(&it, input, 1);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x0A, cp);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_two_byte_pound_sign_returns_ok(void) {
    utf8_iter_t it;
    const char *input = "£";
    utf8_iter_init(&it, input, 2);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0xA3, cp);
    TEST_ASSERT_EQUAL_PTR(input + 2, it.p);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_caps_return_ok);
    RUN_TEST(test_ascii_lowercase_return_ok);
    RUN_TEST(test_ascii_digits_return_ok);
    RUN_TEST(test_ascii_newline_return_ok);
    RUN_TEST(test_two_byte_pound_sign_returns_ok);

    // test_three_byte_euro
    // test_four_byte_musical_clef
    // test_empty_input_returns_end
    // test_invalid_lead_byte
    // test_truncated_two_byte
    // test_bad_continuation_byte
    // test_overlong_null_two_byte
    // test_surrogate_rejected
    // test_codepoint_above_max_rejected
    // test_multiple_codepoints_iterate_correctly
    return UNITY_END();
}
