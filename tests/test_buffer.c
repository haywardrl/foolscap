#include "app/buffer/buffer.h"
#include "unity_internals.h"

#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_create_returns_non_null(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_NOT_NULL(buffer);
    buffer_destroy(buffer);
}

static void test_create_size_zero(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_create_cursor_zero(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_destroy_null_safe(void) {
    buffer_destroy(NULL);
}

static void test_insert_bytes_increases_size(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_insert_bytes_advances_cursor(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_insert_bytes_stores_content(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    buffer_destroy(buffer);
}

static void test_insert_bytes_multibyte_atomic(void) {
    buffer_t *buffer = buffer_create(16);
    // Euro sign: 0xE2 0x82 0xAC (3 bytes)
    const char euro[] = {(char)0xE2, (char)0x82, (char)0xAC};
    TEST_ASSERT_TRUE(buffer_insert_bytes(buffer, euro, 3));
    TEST_ASSERT_EQUAL_size_t(3, buffer_size(buffer));
    TEST_ASSERT_EQUAL((char)0xE2, buffer_char_at(buffer, 0));
    TEST_ASSERT_EQUAL((char)0x82, buffer_char_at(buffer, 1));
    TEST_ASSERT_EQUAL((char)0xAC, buffer_char_at(buffer, 2));
    buffer_destroy(buffer);
}

static void test_insert_bytes_zero_len_succeeds_no_change(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_TRUE(buffer_insert_bytes(buffer, "ignored", 0));
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_insert_bytes_overflow_returns_false(void) {
    buffer_t *buffer = buffer_create(4);
    TEST_ASSERT_FALSE(buffer_insert_bytes(buffer, "ABCDE", 5));
    // Atomic: nothing inserted on failure
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_insert_bytes_exact_fit(void) {
    buffer_t *buffer = buffer_create(4);
    TEST_ASSERT_TRUE(buffer_insert_bytes(buffer, "ABCD", 4));
    TEST_ASSERT_EQUAL_size_t(4, buffer_size(buffer));
    // One more byte fails
    TEST_ASSERT_FALSE(buffer_insert_bytes(buffer, "E", 1));
    buffer_destroy(buffer);
}

static void test_delete_before_cursor_at_start_returns_zero(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_EQUAL_size_t(0, buffer_delete_before_cursor(buffer, 1));
    buffer_destroy(buffer);
}

static void test_delete_before_cursor_one_byte(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    TEST_ASSERT_EQUAL_size_t(1, buffer_delete_before_cursor(buffer, 1));
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buffer));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    buffer_destroy(buffer);
}

static void test_delete_before_cursor_multi_byte(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    TEST_ASSERT_EQUAL_size_t(3, buffer_delete_before_cursor(buffer, 3));
    TEST_ASSERT_EQUAL_size_t(2, buffer_size(buffer));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    TEST_ASSERT_EQUAL('B', buffer_char_at(buffer, 1));
    buffer_destroy(buffer);
}

static void test_delete_before_cursor_clamps_to_available(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    // Ask to delete 10, only 2 available
    TEST_ASSERT_EQUAL_size_t(2, buffer_delete_before_cursor(buffer, 10));
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_delete_after_cursor_at_end_returns_zero(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    // Cursor is at the end after insert
    TEST_ASSERT_EQUAL_size_t(0, buffer_delete_after_cursor(buffer, 1));
    buffer_destroy(buffer);
}

static void test_delete_after_cursor_one_byte(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    buffer_set_cursor(buffer, 0);
    TEST_ASSERT_EQUAL_size_t(1, buffer_delete_after_cursor(buffer, 1));
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buffer));
    TEST_ASSERT_EQUAL('B', buffer_char_at(buffer, 0));
    buffer_destroy(buffer);
}

static void test_delete_after_cursor_multi_byte(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    buffer_set_cursor(buffer, 0);
    TEST_ASSERT_EQUAL_size_t(3, buffer_delete_after_cursor(buffer, 3));
    TEST_ASSERT_EQUAL_size_t(2, buffer_size(buffer));
    TEST_ASSERT_EQUAL('D', buffer_char_at(buffer, 0));
    TEST_ASSERT_EQUAL('E', buffer_char_at(buffer, 1));
    buffer_destroy(buffer);
}

static void test_delete_after_cursor_clamps_to_available(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    buffer_set_cursor(buffer, 0);
    TEST_ASSERT_EQUAL_size_t(2, buffer_delete_after_cursor(buffer, 10));
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_set_cursor_clamps_past_end(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    buffer_set_cursor(buffer, 100);
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_set_cursor_returns_false_when_no_movement(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    // Cursor already at position 1 after insert
    TEST_ASSERT_FALSE(buffer_set_cursor(buffer, 1));
    buffer_destroy(buffer);
}

static void test_insert_after_cursor_movement(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    buffer_set_cursor(buffer, 2);
    buffer_insert_bytes(buffer, "X", 1);
    TEST_ASSERT_EQUAL_size_t(6, buffer_size(buffer));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    TEST_ASSERT_EQUAL('B', buffer_char_at(buffer, 1));
    TEST_ASSERT_EQUAL('X', buffer_char_at(buffer, 2));
    TEST_ASSERT_EQUAL('C', buffer_char_at(buffer, 3));
    TEST_ASSERT_EQUAL('D', buffer_char_at(buffer, 4));
    TEST_ASSERT_EQUAL('E', buffer_char_at(buffer, 5));
    buffer_destroy(buffer);
}

static void test_char_at_out_of_range_returns_null_byte(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL('\0', buffer_char_at(buffer, 100));
    buffer_destroy(buffer);
}

static void test_prev_boundary_at_zero(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL_size_t(0, buffer_prev_codepoint_boundary(buffer, 0));
    buffer_destroy(buffer);
}

static void test_prev_boundary_ascii(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    TEST_ASSERT_EQUAL_size_t(1, buffer_prev_codepoint_boundary(buffer, 2));
    TEST_ASSERT_EQUAL_size_t(0, buffer_prev_codepoint_boundary(buffer, 1));
    buffer_destroy(buffer);
}

static void test_prev_boundary_multibyte(void) {
    buffer_t *buffer = buffer_create(16);
    // 'A' + Euro (3 bytes) + 'B' = byte positions: 0='A', 1-3=Euro, 4='B'
    const char input[] = {'A', (char)0xE2, (char)0x82, (char)0xAC, 'B'};
    buffer_insert_bytes(buffer, input, 5);
    // From 'B' (pos 4), prev boundary is start of Euro (pos 1)
    TEST_ASSERT_EQUAL_size_t(1, buffer_prev_codepoint_boundary(buffer, 4));
    // From start of Euro (pos 1), prev is 'A' (pos 0)
    TEST_ASSERT_EQUAL_size_t(0, buffer_prev_codepoint_boundary(buffer, 1));
    buffer_destroy(buffer);
}

static void test_next_boundary_at_end(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "A", 1);
    TEST_ASSERT_EQUAL_size_t(1, buffer_next_codepoint_boundary(buffer, 1));
    buffer_destroy(buffer);
}

static void test_next_boundary_ascii(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "AB", 2);
    TEST_ASSERT_EQUAL_size_t(1, buffer_next_codepoint_boundary(buffer, 0));
    TEST_ASSERT_EQUAL_size_t(2, buffer_next_codepoint_boundary(buffer, 1));
    buffer_destroy(buffer);
}

static void test_next_boundary_multibyte(void) {
    buffer_t *buffer = buffer_create(16);
    const char input[] = {'A', (char)0xE2, (char)0x82, (char)0xAC, 'B'};
    buffer_insert_bytes(buffer, input, 5);
    // From 'A' (pos 0), next is start of Euro (pos 1)
    TEST_ASSERT_EQUAL_size_t(1, buffer_next_codepoint_boundary(buffer, 0));
    // From start of Euro (pos 1), next is 'B' (pos 4)
    TEST_ASSERT_EQUAL_size_t(4, buffer_next_codepoint_boundary(buffer, 1));
    buffer_destroy(buffer);
}

static void test_copy_contiguous_empty(void) {
    buffer_t *buffer = buffer_create(16);
    char dst[16];
    TEST_ASSERT_EQUAL_size_t(0, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    buffer_destroy(buffer);
}

static void test_copy_contiguous_gap_at_end(void) {
    // Cursor at end of inserted content = gap at end
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    char dst[16] = {0};
    TEST_ASSERT_EQUAL_size_t(5, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    TEST_ASSERT_EQUAL_MEMORY("ABCDE", dst, 5);
    buffer_destroy(buffer);
}

static void test_copy_contiguous_gap_at_start(void) {
    // Move cursor to 0 so the gap is at the start, content is in the after-gap region
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    buffer_set_cursor(buffer, 0);
    char dst[16] = {0};
    TEST_ASSERT_EQUAL_size_t(5, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    TEST_ASSERT_EQUAL_MEMORY("ABCDE", dst, 5);
    buffer_destroy(buffer);
}

static void test_copy_contiguous_gap_in_middle(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    buffer_set_cursor(buffer, 2); // gap now between 'B' and 'C'
    char dst[16] = {0};
    TEST_ASSERT_EQUAL_size_t(5, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    TEST_ASSERT_EQUAL_MEMORY("ABCDE", dst, 5);
    buffer_destroy(buffer);
}

static void test_copy_contiguous_dst_too_small(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    char dst[3] = {'X', 'X', 'X'};
    TEST_ASSERT_EQUAL_size_t(0, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    // Nothing written
    TEST_ASSERT_EQUAL('X', dst[0]);
    TEST_ASSERT_EQUAL('X', dst[1]);
    TEST_ASSERT_EQUAL('X', dst[2]);
    buffer_destroy(buffer);
}

static void test_copy_contiguous_exact_fit(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_bytes(buffer, "ABCDE", 5);
    char dst[5];
    TEST_ASSERT_EQUAL_size_t(5, buffer_copy_contiguous(buffer, dst, sizeof(dst)));
    TEST_ASSERT_EQUAL_MEMORY("ABCDE", dst, 5);
    buffer_destroy(buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_returns_non_null);
    RUN_TEST(test_create_size_zero);
    RUN_TEST(test_create_cursor_zero);
    RUN_TEST(test_destroy_null_safe);

    RUN_TEST(test_insert_bytes_increases_size);
    RUN_TEST(test_insert_bytes_advances_cursor);
    RUN_TEST(test_insert_bytes_stores_content);
    RUN_TEST(test_insert_bytes_multibyte_atomic);
    RUN_TEST(test_insert_bytes_zero_len_succeeds_no_change);
    RUN_TEST(test_insert_bytes_overflow_returns_false);
    RUN_TEST(test_insert_bytes_exact_fit);

    RUN_TEST(test_delete_before_cursor_at_start_returns_zero);
    RUN_TEST(test_delete_before_cursor_one_byte);
    RUN_TEST(test_delete_before_cursor_multi_byte);
    RUN_TEST(test_delete_before_cursor_clamps_to_available);

    RUN_TEST(test_delete_after_cursor_clamps_to_available);
    RUN_TEST(test_delete_after_cursor_at_end_returns_zero);
    RUN_TEST(test_delete_after_cursor_multi_byte);
    RUN_TEST(test_delete_after_cursor_one_byte);

    RUN_TEST(test_set_cursor_clamps_past_end);
    RUN_TEST(test_set_cursor_returns_false_when_no_movement);
    RUN_TEST(test_insert_after_cursor_movement);

    RUN_TEST(test_char_at_out_of_range_returns_null_byte);

    RUN_TEST(test_prev_boundary_at_zero);
    RUN_TEST(test_prev_boundary_ascii);
    RUN_TEST(test_prev_boundary_multibyte);
    RUN_TEST(test_next_boundary_at_end);
    RUN_TEST(test_next_boundary_ascii);
    RUN_TEST(test_next_boundary_multibyte);

    RUN_TEST(test_copy_contiguous_empty);
    RUN_TEST(test_copy_contiguous_gap_at_end);
    RUN_TEST(test_copy_contiguous_gap_at_start);
    RUN_TEST(test_copy_contiguous_gap_in_middle);
    RUN_TEST(test_copy_contiguous_dst_too_small);
    RUN_TEST(test_copy_contiguous_exact_fit);

    return UNITY_END();
}
