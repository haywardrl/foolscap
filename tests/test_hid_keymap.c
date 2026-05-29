#include "ports/esp32s3/hid_keymap.h"

#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_lowercase_letter(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x04, false, &e)); // 'a'
    TEST_ASSERT_EQUAL(KEY_INSERT, e.kind);
    TEST_ASSERT_EQUAL_UINT8(1, e.len);
    TEST_ASSERT_EQUAL('a', e.bytes[0]);
}

static void test_shift_uppercases(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x02, 0x04, false, &e)); // left-shift + 'a'
    TEST_ASSERT_EQUAL('A', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x20, 0x04, false, &e)); // right-shift + 'a'
    TEST_ASSERT_EQUAL('A', e.bytes[0]);
}

static void test_shifted_digit_is_symbol(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x1E, false, &e)); // '1'
    TEST_ASSERT_EQUAL('1', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x02, 0x1E, false, &e)); // shift+1 -> '!'
    TEST_ASSERT_EQUAL('!', e.bytes[0]);
}

static void test_zero_and_its_symbol(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x27, false, &e));
    TEST_ASSERT_EQUAL('0', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x02, 0x27, false, &e));
    TEST_ASSERT_EQUAL(')', e.bytes[0]);
}

static void test_enter_inserts_newline(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x28, false, &e));
    TEST_ASSERT_EQUAL(KEY_INSERT, e.kind);
    TEST_ASSERT_EQUAL(1, e.len);
    TEST_ASSERT_EQUAL('\n', e.bytes[0]);
}

static void test_editing_and_navigation_keys(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x2A, false, &e));
    TEST_ASSERT_EQUAL(KEY_BACKSPACE, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x4C, false, &e));
    TEST_ASSERT_EQUAL(KEY_DELETE, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x4F, false, &e));
    TEST_ASSERT_EQUAL(KEY_RIGHT, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x50, false, &e));
    TEST_ASSERT_EQUAL(KEY_LEFT, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x51, false, &e));
    TEST_ASSERT_EQUAL(KEY_DOWN, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x52, false, &e));
    TEST_ASSERT_EQUAL(KEY_UP, e.kind);
}

static void test_unhandled_keys_rejected(void) {
    key_event_t e;
    TEST_ASSERT_FALSE(hid_decode(0x00, 0x00, false, &e)); // no key
    TEST_ASSERT_FALSE(hid_decode(0x00, 0x29, false, &e)); // Esc, not mapped
    TEST_ASSERT_FALSE(hid_decode(0x00, 0xFF, false, &e)); // out of range
}

static void test_caps_lock_uppercases_letters(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x04, true, &e)); // caps + 'a' -> 'A'
    TEST_ASSERT_EQUAL('A', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x1D, true, &e)); // caps + 'z' -> 'Z'
    TEST_ASSERT_EQUAL('Z', e.bytes[0]);
}

static void test_caps_lock_xors_with_shift(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x02, 0x04, true, &e)); // caps + shift + 'a' -> 'a'
    TEST_ASSERT_EQUAL('a', e.bytes[0]);
}

static void test_caps_lock_leaves_digits_and_punct_alone(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x1E, true, &e)); // caps + '1' -> '1'
    TEST_ASSERT_EQUAL('1', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x02, 0x1E, true, &e)); // caps + shift + '1' -> '!'
    TEST_ASSERT_EQUAL('!', e.bytes[0]);
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x36, true, &e)); // caps + ',' -> ','
    TEST_ASSERT_EQUAL(',', e.bytes[0]);
}

static void test_ctrl_arrow_is_word_motion(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x01, 0x4F, false, &e)); // L-Ctrl + Right
    TEST_ASSERT_EQUAL(KEY_WORD_RIGHT, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x10, 0x50, false, &e)); // R-Ctrl + Left
    TEST_ASSERT_EQUAL(KEY_WORD_LEFT, e.kind);
}

static void test_alt_arrow_is_word_motion(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x04, 0x4F, false, &e)); // L-Alt + Right
    TEST_ASSERT_EQUAL(KEY_WORD_RIGHT, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x40, 0x50, false, &e)); // R-Alt + Left
    TEST_ASSERT_EQUAL(KEY_WORD_LEFT, e.kind);
}

static void test_alt_f_and_alt_b_are_emacs_word_motion(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x04, 0x09, false, &e)); // Alt + f
    TEST_ASSERT_EQUAL(KEY_WORD_RIGHT, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x04, 0x05, false, &e)); // Alt + b
    TEST_ASSERT_EQUAL(KEY_WORD_LEFT, e.kind);
}

static void test_alt_other_letter_is_swallowed(void) {
    key_event_t e;
    // Alt + 'a' must not insert 'a'
    TEST_ASSERT_FALSE(hid_decode(0x04, 0x04, false, &e));
}

static void test_ctrl_z_and_ctrl_y_are_undo_redo(void) {
    key_event_t e;
    TEST_ASSERT_TRUE(hid_decode(0x01, 0x1D, false, &e)); // L-Ctrl + z
    TEST_ASSERT_EQUAL(KEY_UNDO, e.kind);
    TEST_ASSERT_TRUE(hid_decode(0x10, 0x1C, false, &e)); // R-Ctrl + y
    TEST_ASSERT_EQUAL(KEY_REDO, e.kind);
    // without ctrl they must type the letter, not undo/redo
    TEST_ASSERT_TRUE(hid_decode(0x00, 0x1D, false, &e));
    TEST_ASSERT_EQUAL(KEY_INSERT, e.kind);
    TEST_ASSERT_EQUAL('z', e.bytes[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lowercase_letter);
    RUN_TEST(test_shift_uppercases);
    RUN_TEST(test_shifted_digit_is_symbol);
    RUN_TEST(test_zero_and_its_symbol);
    RUN_TEST(test_enter_inserts_newline);
    RUN_TEST(test_editing_and_navigation_keys);
    RUN_TEST(test_unhandled_keys_rejected);
    RUN_TEST(test_caps_lock_uppercases_letters);
    RUN_TEST(test_caps_lock_xors_with_shift);
    RUN_TEST(test_caps_lock_leaves_digits_and_punct_alone);
    RUN_TEST(test_ctrl_arrow_is_word_motion);
    RUN_TEST(test_alt_arrow_is_word_motion);
    RUN_TEST(test_alt_f_and_alt_b_are_emacs_word_motion);
    RUN_TEST(test_alt_other_letter_is_swallowed);
    RUN_TEST(test_ctrl_z_and_ctrl_y_are_undo_redo);
    return UNITY_END();
}
