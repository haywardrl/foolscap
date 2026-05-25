#include "app/layout/layout.h"
#include "app/render/font.h"

#include <unity.h>

// x_advance=10, line_height=20; one glyph entry is enough layout only reads glyphs[0]
static const font_glyph_t glyphs[] = {{.codepoint = 'A', .x_advance = 10}};
static const font_t font = {.glyphs = glyphs, .glyph_count = 1, .line_height = 20};

#define WRAP 50
#define MX 0
#define MY 0
#define LH 20

void setUp(void) {
}
void tearDown(void) {
}

static void test_empty_text_produces_one_empty_line(void) {
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, "", 0, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(1, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_INT(MY, l.lines[0].y);
    TEST_ASSERT_FALSE(l.lines[0].ends_with_newline);
    layout_destroy(&l);
}

static void test_single_line_no_wrap(void) {
    const char *text = "ABC";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 3, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(1, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[0].byte_end);
    TEST_ASSERT_FALSE(l.lines[0].ends_with_newline);
    layout_destroy(&l);
}

static void test_newline_splits_into_two_lines(void) {
    // "AB\nCD": line 0 = "AB" [0,2), line 1 = "CD" [3,5)
    // byte_end excludes the \n itself
    const char *text = "AB\nCD";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 5, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);

    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_INT(MY, l.lines[0].y);
    TEST_ASSERT_TRUE(l.lines[0].ends_with_newline);

    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(5, l.lines[1].byte_end);
    TEST_ASSERT_EQUAL_INT(MY + LH, l.lines[1].y);
    TEST_ASSERT_FALSE(l.lines[1].ends_with_newline);

    layout_destroy(&l);
}

static void test_trailing_newline_produces_empty_final_line(void) {
    // "AB\n": line 0 = "AB" [0,2) newline, line 1 = "" [3,3)
    const char *text = "AB\n";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 3, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[0].byte_end);
    TEST_ASSERT_TRUE(l.lines[0].ends_with_newline);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_end);
    layout_destroy(&l);
}

static void test_leading_newline_produces_empty_first_line(void) {
    // "\nA": line 0 = "" [0,0), line 1 = "A" [1,2)
    const char *text = "\nA";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 2, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_end);
    TEST_ASSERT_TRUE(l.lines[0].ends_with_newline);
    TEST_ASSERT_EQUAL_size_t(1, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[1].byte_end);
    layout_destroy(&l);
}

static void test_char_wrap_at_boundary(void) {
    // "ABCDEF" wrap=30 (3 chars): line 0=[0,3), line 1=[3,6)
    const char *text = "ABCDEF";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 6, &font, 30, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_INT(MY, l.lines[0].y);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(6, l.lines[1].byte_end);
    TEST_ASSERT_EQUAL_INT(MY + LH, l.lines[1].y);
    layout_destroy(&l);
}

static void test_exact_wrap_width_does_not_wrap(void) {
    // x_advance=10, wrap=30, "ABC" x reaches exactly 30, not > 30, stays on one line
    const char *text = "ABC";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 3, &font, 30, MX, MY));
    TEST_ASSERT_EQUAL_size_t(1, l.count);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[0].byte_end);
    layout_destroy(&l);
}

static void test_word_wrap_breaks_at_space(void) {
    // "AB CD" wrap=40: "AB" [0,2), "CD" [3,5) space at byte 2 is dropped
    const char *text = "AB CD";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 5, &font, 40, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(5, l.lines[1].byte_end);
    TEST_ASSERT_EQUAL_INT(MY + LH, l.lines[1].y);
    layout_destroy(&l);
}

static void test_word_too_long_falls_back_to_char_wrap(void) {
    // "ABCDE" wrap=30, no spaces: char wrap at 3. line 0=[0,3), line 1=[3,5)
    const char *text = "ABCDE";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 5, &font, 30, MX, MY));
    TEST_ASSERT_EQUAL_size_t(2, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(5, l.lines[1].byte_end);
    layout_destroy(&l);
}

static void test_multiple_wraps(void) {
    // "ABCD EFGH IJ" wrap=40: "ABCD" [0,4), "EFGH" [5,9), "IJ" [10,12)
    const char *text = "ABCD EFGH IJ";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 12, &font, 40, MX, MY));
    TEST_ASSERT_EQUAL_size_t(3, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(4, l.lines[0].byte_end);
    TEST_ASSERT_EQUAL_size_t(5, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(9, l.lines[1].byte_end);
    TEST_ASSERT_EQUAL_size_t(10, l.lines[2].byte_start);
    TEST_ASSERT_EQUAL_size_t(12, l.lines[2].byte_end);
    layout_destroy(&l);
}

static void test_newline_then_wrap(void) {
    // "AB\nCDEFG" wrap=30: "AB" [0,2)\n, "CDE" [3,6), "FG" [6,8)
    const char *text = "AB\nCDEFG";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 8, &font, 30, MX, MY));
    TEST_ASSERT_EQUAL_size_t(3, l.count);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[0].byte_end);
    TEST_ASSERT_TRUE(l.lines[0].ends_with_newline);
    TEST_ASSERT_EQUAL_size_t(3, l.lines[1].byte_start);
    TEST_ASSERT_EQUAL_size_t(6, l.lines[1].byte_end);
    TEST_ASSERT_EQUAL_INT(MY + LH, l.lines[1].y);
    TEST_ASSERT_EQUAL_size_t(6, l.lines[2].byte_start);
    TEST_ASSERT_EQUAL_size_t(8, l.lines[2].byte_end);
    TEST_ASSERT_EQUAL_INT(MY + LH * 2, l.lines[2].y);
    layout_destroy(&l);
}

static void test_invalid_utf8_byte_skipped(void) {
    // "A\xC0" \xC0 is an invalid UTF-8 lead byte; iterator returns INVALID and skips.
    const char text[] = {'A', (char)0xC0};
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 2, &font, WRAP, MX, MY));
    TEST_ASSERT_EQUAL_size_t(1, l.count);
    TEST_ASSERT_EQUAL_size_t(0, l.lines[0].byte_start);
    TEST_ASSERT_EQUAL_size_t(2, l.lines[0].byte_end);
    layout_destroy(&l);
}

static void test_y_advances_per_line(void) {
    const char *text = "A\nB\nC";
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, text, 5, &font, WRAP, MX, 10));
    TEST_ASSERT_EQUAL_size_t(3, l.count);
    TEST_ASSERT_EQUAL_INT(10, l.lines[0].y);
    TEST_ASSERT_EQUAL_INT(10 + LH, l.lines[1].y);
    TEST_ASSERT_EQUAL_INT(10 + LH * 2, l.lines[2].y);
    layout_destroy(&l);
}

static void test_destroy_safe_on_zeroed_layout(void) {
    layout_t l = {0};
    layout_destroy(&l); // must not crash
    TEST_ASSERT_NULL(l.lines);
    TEST_ASSERT_EQUAL_size_t(0, l.count);
}

static void test_destroy_idempotent(void) {
    layout_t l;
    TEST_ASSERT_TRUE(layout_compute(&l, "AB", 2, &font, WRAP, MX, MY));
    layout_destroy(&l);
    layout_destroy(&l); // second call must be safe
    TEST_ASSERT_NULL(l.lines);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_text_produces_one_empty_line);
    RUN_TEST(test_single_line_no_wrap);
    RUN_TEST(test_newline_splits_into_two_lines);
    RUN_TEST(test_trailing_newline_produces_empty_final_line);
    RUN_TEST(test_leading_newline_produces_empty_first_line);
    RUN_TEST(test_char_wrap_at_boundary);
    RUN_TEST(test_exact_wrap_width_does_not_wrap);
    RUN_TEST(test_word_wrap_breaks_at_space);
    RUN_TEST(test_word_too_long_falls_back_to_char_wrap);
    RUN_TEST(test_multiple_wraps);
    RUN_TEST(test_newline_then_wrap);
    RUN_TEST(test_invalid_utf8_byte_skipped);
    RUN_TEST(test_y_advances_per_line);
    RUN_TEST(test_destroy_safe_on_zeroed_layout);
    RUN_TEST(test_destroy_idempotent);
    return UNITY_END();
}
