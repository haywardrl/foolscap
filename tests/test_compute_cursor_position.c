#include "app/render/font.h"
#include "app/render/render.h"

#include <stddef.h>
#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

// advance-only font for position math
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

#define START_X 10
#define BASELINE_Y 30

static void test_empty_text_returns_start_position(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "", 0, 0, &x, &y);
    TEST_ASSERT_EQUAL(START_X, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_at_zero_returns_start_position(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "AB", 2, 0, &x, &y);
    TEST_ASSERT_EQUAL(START_X, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_after_single_char(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "A", 1, 1, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_after_two_chars(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "AB", 2, 2, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance + glyphs[1].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_mid_text(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "AB", 2, 1, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_after_newline(void) {
    int x = 0, y = 0;
    // "A\n" with cursor at byte 2 (after the newline)
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "A\n", 2, 2, &x, &y);
    TEST_ASSERT_EQUAL(START_X, x);
    TEST_ASSERT_EQUAL(BASELINE_Y + font.line_height, y);
}

static void test_cursor_after_newline_then_char(void) {
    int x = 0, y = 0;
    // "A\nB" with cursor at byte 3 (after the B on the second line)
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "A\nB", 3, 3, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[1].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y + font.line_height, y);
}

static void test_cursor_after_multibyte_codepoint(void) {
    // 'A' + Euro (3 bytes) — cursor at byte 4 (after the Euro)
    const char input[] = {'A', (char)0xE2, (char)0x82, (char)0xAC};
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, input, 4, 4, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance + glyphs[2].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_at_codepoint_boundary_inside_multibyte_text(void) {
    // 'A' + Euro — cursor at byte 1 (between A and Euro)
    const char input[] = {'A', (char)0xE2, (char)0x82, (char)0xAC};
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, input, 4, 1, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

static void test_cursor_byte_past_len_clamps(void) {
    int x = 0, y = 0;
    render_compute_cursor_position(&font, START_X, BASELINE_Y, "AB", 2, 100, &x, &y);
    TEST_ASSERT_EQUAL(START_X + glyphs[0].x_advance + glyphs[1].x_advance, x);
    TEST_ASSERT_EQUAL(BASELINE_Y, y);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_text_returns_start_position);
    RUN_TEST(test_cursor_at_zero_returns_start_position);
    RUN_TEST(test_cursor_after_single_char);
    RUN_TEST(test_cursor_after_two_chars);
    RUN_TEST(test_cursor_mid_text);
    RUN_TEST(test_cursor_after_newline);
    RUN_TEST(test_cursor_after_newline_then_char);
    RUN_TEST(test_cursor_after_multibyte_codepoint);
    RUN_TEST(test_cursor_at_codepoint_boundary_inside_multibyte_text);
    RUN_TEST(test_cursor_byte_past_len_clamps);
    UNITY_END();
}
