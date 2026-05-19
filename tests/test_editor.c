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

// static void test_editor_insert_utf8_overflow_returns_false_and_no_dirty(void) {
//     editor_t *ed = editor_create(&font, 256);
//     TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
//     const buffer_t *buf = editor_test_get_buffer(ed);
//     TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
//     TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
//     TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buf));
//     TEST_ASSERT_TRUE(editor_is_dirty(ed));
//     editor_destroy(ed);
// }

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
    return UNITY_END();
}
