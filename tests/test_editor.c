#include "app/buffer/buffer.h"
#include "app/editor/editor.h"
#include "app/editor/editor_test.h"
#include "app/render/font.h"
#include "mocks/display_mock.h"
#include "unity_internals.h"

#include <stdint.h>
#include <unity.h>

static const font_glyph_t glyphs[] = {
    {.codepoint = 'A', .x_advance = 10},
    {.codepoint = 'B', .x_advance = 7},
    {.codepoint = 0x20AC, .x_advance = 13}, // Euro
};
static const font_t font = {
    .glyphs = glyphs,
    .glyph_count = 3,
    .line_height = 20,
};

void setUp(void) {
    display_mock_reset();
}

void tearDown(void) {
}

static void test_editor_create_returns_non_null_for_valid_args(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_NOT_NULL(ed);
    editor_destroy(ed);
}

static void test_editor_create_returns_null_for_null_font(void) {
    TEST_ASSERT_NULL(editor_create(NULL, 256));
}

static void test_editor_create_returns_null_for_zero_capacity(void) {
    TEST_ASSERT_NULL(editor_create(&font, 0));
}

static void test_editor_create_returns_null_for_font_with_no_glyphs(void) {
    static const font_t font_no_glyphs = {
        .glyphs = NULL,
        .glyph_count = 0,
        .line_height = 20,
    };
    TEST_ASSERT_NULL(editor_create(&font_no_glyphs, 256));
}

static void test_editor_destroy_null_safe(void) {
    editor_destroy(NULL); // Should not crash
}

static void test_editor_insert_utf8_updates_buffer(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_insert_utf8_sets_dirty(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buf));
    TEST_ASSERT_TRUE(editor_is_dirty(ed));
    editor_destroy(ed);
}

static void test_editor_insert_utf8_overflow_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_FALSE(editor_insert_utf8(ed, "AB", 2));
    editor_destroy(ed);
}

static void test_editor_backspace_at_zero_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buf));
    TEST_ASSERT_FALSE(editor_backspace(ed));
    editor_destroy(ed);
}

static void test_editor_backspace_deletes_one_codepoint_ascii(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_TRUE(editor_backspace(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_backspace_deletes_one_codepoint_multibyte_euro(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3); // 3-byte insert (Euro)
    TEST_ASSERT_TRUE(editor_backspace(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_delete_forward_at_end_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_FALSE(editor_delete_forward(ed));
    editor_destroy(ed);
}

static void test_editor_delete_forward_deletes_one_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
    TEST_ASSERT_TRUE(editor_delete_forward(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_move_cursor_left_at_zero_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    editor_destroy(ed);
}

static void test_editor_move_cursor_right_at_end_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_RIGHT));
    editor_destroy(ed);
}

static void test_editor_move_cursor_left_advances_by_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_move_cursor_right_advances_by_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_RIGHT));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(6, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_render_clears_dirty_flag(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_NOT_NULL(ed);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_TRUE(editor_is_dirty(ed));
    editor_render(ed);
    TEST_ASSERT_FALSE(editor_is_dirty(ed));
    editor_destroy(ed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_editor_create_returns_non_null_for_valid_args);
    RUN_TEST(test_editor_render_clears_dirty_flag);
    RUN_TEST(test_editor_create_returns_null_for_null_font);
    RUN_TEST(test_editor_create_returns_null_for_zero_capacity);
    RUN_TEST(test_editor_create_returns_null_for_font_with_no_glyphs);
    RUN_TEST(test_editor_destroy_null_safe);
    RUN_TEST(test_editor_insert_utf8_updates_buffer);
    RUN_TEST(test_editor_insert_utf8_sets_dirty);
    RUN_TEST(test_editor_insert_utf8_overflow_returns_false);
    RUN_TEST(test_editor_backspace_at_zero_returns_false);
    RUN_TEST(test_editor_backspace_deletes_one_codepoint_ascii);
    RUN_TEST(test_editor_backspace_deletes_one_codepoint_multibyte_euro);
    RUN_TEST(test_editor_delete_forward_at_end_returns_false);
    RUN_TEST(test_editor_delete_forward_deletes_one_codepoint_multibyte);
    RUN_TEST(test_editor_move_cursor_left_at_zero_returns_false);
    RUN_TEST(test_editor_move_cursor_right_at_end_returns_false);
    RUN_TEST(test_editor_move_cursor_left_advances_by_codepoint_multibyte);
    RUN_TEST(test_editor_move_cursor_right_advances_by_codepoint_multibyte);
    return UNITY_END();
}
