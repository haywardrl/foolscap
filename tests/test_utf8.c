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

static void test_three_byte_euro(void) {
    utf8_iter_t it;
    const char *input = "€";
    utf8_iter_init(&it, input, 3);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x20AC, cp);
    TEST_ASSERT_EQUAL_PTR(input + 3, it.p);
}

static void test_four_byte_musical_clef(void) {
    utf8_iter_t it;
    const char *input = "𝄞";
    utf8_iter_init(&it, input, 4);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_OK, status);
    TEST_ASSERT_EQUAL_HEX32(0x1D11E, cp);
    TEST_ASSERT_EQUAL_PTR(input + 4, it.p);
}

static void test_empty_input_returns_end(void) {
    utf8_iter_t it;
    const char *input = "";
    utf8_iter_init(&it, input, 0);
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_END, status);
    TEST_ASSERT_EQUAL_PTR(input, it.p);
}

static void test_invalid_lead_byte(void) {
    // 0xFF is never a valid UTF-8 lead byte
    const char input[] = {(char)0xFF};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

static void test_invalid_then_valid(void) {
    // Invalid byte followed by ASCII 'A'
    const char input[] = {(char)0xFF, 0x41};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0;
    // First call: invalid, advances by 1
    TEST_ASSERT_EQUAL(UTF8_INVALID, utf8_next(&it, &cp));
    // Second call: should now decode the 'A'
    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0x41, cp);
    // Third call: end of input
    TEST_ASSERT_EQUAL(UTF8_END, utf8_next(&it, &cp));
}

static void test_truncated_two_byte(void) {
    // Lead byte claims 2-byte sequence but no continuation byte follows
    const char input[] = {(char)0xC2};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

static void test_bad_continuation_byte(void) {
    const char input[] = {(char)0xFF, 0x01};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_overlong_null_two_byte(void) {
    const char input[] = {(char)0xC0, (char)0x80};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
}

static void test_surrogate_rejected(void) {
    // U+D800 — high surrogate, invalid in UTF-8
    // ED A0 80 would decode to D800 if not for the surrogate check
    const char input[] = {(char)0xED, (char)0xA0, (char)0x80};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

static void test_codepoint_above_max_rejected(void) {
    // U+110000 — one above the max valid codepoint (U+10FFFF)
    // F4 90 80 80 would decode to 110000 if not for the range check
    const char input[] = {(char)0xF4, (char)0x90, (char)0x80, (char)0x80};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

// 0x80 is a valid continuation byte but never a valid lead.
// Some naive decoders mishandle this distinctly from 0xFF.
static void test_orphan_continuation_byte(void) {
    const char input[] = {(char)0x80};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

// Lead bytes 0xF8–0xFD encoded 5- and 6-byte sequences in obsolete UTF-8.
// Modern UTF-8 (RFC 3629) caps at 4 bytes. Reject these lead bytes.
static void test_obsolete_five_byte_lead_rejected(void) {
    const char input[] = {(char)0xF8, (char)0x80, (char)0x80, (char)0x80, (char)0x80};
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0xDEADBEEF;
    utf8_status_t status = utf8_next(&it, &cp);
    TEST_ASSERT_EQUAL(UTF8_INVALID, status);
    TEST_ASSERT_EQUAL_PTR(input + 1, it.p);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, cp);
}

// Re-initialising an iterator should fully reset its state.
static void test_iter_init_resets_state(void) {
    utf8_iter_t it;
    const char *first = "A";
    utf8_iter_init(&it, first, 1);
    uint32_t cp = 0;
    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL(UTF8_END, utf8_next(&it, &cp));

    // Re-init with new input — iterator must point at the new buffer,
    // not be stuck at the end of the old one.
    const char *second = "B";
    utf8_iter_init(&it, second, 1);
    TEST_ASSERT_EQUAL_PTR(second, it.p);
    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0x42, cp);
}

static void test_multiple_codepoints_iterate_correctly(void) {
    // "A£€𝄞" — one each of 1, 2, 3, 4-byte sequences
    // A     = 0x41                    (1 byte)
    // £     = 0xC2 0xA3               (2 bytes)
    // €     = 0xE2 0x82 0xAC          (3 bytes)
    // 𝄞    = 0xF0 0x9D 0x84 0x9E     (4 bytes)
    const char input[] = {
        0x41,       (char)0xC2, (char)0xA3, (char)0xE2, (char)0x82,
        (char)0xAC, (char)0xF0, (char)0x9D, (char)0x84, (char)0x9E,
    };
    utf8_iter_t it;
    utf8_iter_init(&it, input, sizeof(input));
    uint32_t cp = 0;

    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0x41, cp);

    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0xA3, cp);

    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0x20AC, cp);

    TEST_ASSERT_EQUAL(UTF8_OK, utf8_next(&it, &cp));
    TEST_ASSERT_EQUAL_HEX32(0x1D11E, cp);

    TEST_ASSERT_EQUAL(UTF8_END, utf8_next(&it, &cp));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_caps_return_ok);
    RUN_TEST(test_ascii_lowercase_return_ok);
    RUN_TEST(test_ascii_digits_return_ok);
    RUN_TEST(test_ascii_newline_return_ok);
    RUN_TEST(test_two_byte_pound_sign_returns_ok);
    RUN_TEST(test_three_byte_euro);
    RUN_TEST(test_four_byte_musical_clef);
    RUN_TEST(test_empty_input_returns_end);
    RUN_TEST(test_invalid_lead_byte);
    RUN_TEST(test_invalid_then_valid);
    RUN_TEST(test_truncated_two_byte);
    RUN_TEST(test_bad_continuation_byte);
    RUN_TEST(test_overlong_null_two_byte);
    RUN_TEST(test_surrogate_rejected);
    RUN_TEST(test_codepoint_above_max_rejected);
    RUN_TEST(test_orphan_continuation_byte);
    RUN_TEST(test_obsolete_five_byte_lead_rejected);
    RUN_TEST(test_iter_init_resets_state);
    RUN_TEST(test_multiple_codepoints_iterate_correctly);
    return UNITY_END();
}
