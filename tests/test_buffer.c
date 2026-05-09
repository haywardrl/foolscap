#include "app/buffer/buffer.h"

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
    buffer_t *buffer = buffer_create(0);
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

static void test_insert_increases_size(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_insert_advances_cursor(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_insert_stores_char(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    buffer_destroy(buffer);
}

static void test_backspace_at_start_returns_false(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_FALSE(buffer_backspace(buffer));
    buffer_destroy(buffer);
}

static void test_backspace_decreases_size(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    buffer_backspace(buffer);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_delete_forward_at_end_returns_false(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_FALSE(buffer_delete_forward(buffer));
    buffer_destroy(buffer);
}

static void test_delete_forward_decreases_size(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    buffer_set_cursor(buffer, 0);
    buffer_delete_forward(buffer);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buffer));
    buffer_destroy(buffer);
}

static void test_set_cursor_clamps_past_end(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    buffer_set_cursor(buffer, 100);
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_move_cursor_negative_clamps_to_zero(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    buffer_move_cursor(buffer, -100);
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buffer));
    buffer_destroy(buffer);
}

static void test_move_cursor_returns_false_when_no_movement(void) {
    buffer_t *buffer = buffer_create(16);
    TEST_ASSERT_FALSE(buffer_move_cursor(buffer, -5));
    buffer_destroy(buffer);
}

static void test_insert_after_cursor_movement(void) {
    buffer_t *buffer = buffer_create(16);
    buffer_insert_char(buffer, 'A');
    buffer_insert_char(buffer, 'B');
    buffer_insert_char(buffer, 'C');
    buffer_insert_char(buffer, 'D');
    buffer_insert_char(buffer, 'E');
    buffer_set_cursor(buffer, 2);
    buffer_insert_char(buffer, 'X');
    TEST_ASSERT_EQUAL_size_t(6, buffer_size(buffer));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buffer, 0));
    TEST_ASSERT_EQUAL('B', buffer_char_at(buffer, 1));
    TEST_ASSERT_EQUAL('X', buffer_char_at(buffer, 2));
    TEST_ASSERT_EQUAL('C', buffer_char_at(buffer, 3));
    TEST_ASSERT_EQUAL('D', buffer_char_at(buffer, 4));
    TEST_ASSERT_EQUAL('E', buffer_char_at(buffer, 5));
    buffer_destroy(buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_returns_non_null);
    RUN_TEST(test_create_size_zero);
    RUN_TEST(test_create_cursor_zero);
    RUN_TEST(test_destroy_null_safe);
    RUN_TEST(test_insert_increases_size);
    RUN_TEST(test_insert_advances_cursor);
    RUN_TEST(test_insert_stores_char);
    RUN_TEST(test_backspace_at_start_returns_false);
    RUN_TEST(test_backspace_decreases_size);
    RUN_TEST(test_delete_forward_at_end_returns_false);
    RUN_TEST(test_delete_forward_decreases_size);
    RUN_TEST(test_set_cursor_clamps_past_end);
    RUN_TEST(test_move_cursor_negative_clamps_to_zero);
    RUN_TEST(test_move_cursor_returns_false_when_no_movement);
    RUN_TEST(test_insert_after_cursor_movement);
    return UNITY_END();
}
