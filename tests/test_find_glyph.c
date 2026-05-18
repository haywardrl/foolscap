#include "app/render/render.h"
#include "app/render/render_internal.h"

#include <stdint.h>
#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

// --- Fixtures ---

// Empty font: no glyphs at all.
static const font_t font_empty = {
    .glyphs = NULL,
    .glyph_count = 0,
    .line_height = 0,
    .bitmap_data = NULL,
    .bitmap_data_size = 0,
};

// Single-glyph font: just 'M' (mid-range codepoint).
static const font_glyph_t single_glyphs[] = {
    {.codepoint = 'M', .x_advance = 10},
};
static const font_t font_single = {
    .glyphs = single_glyphs,
    .glyph_count = 1,
};

// Multi-glyph font: A, B, C, D, E — contiguous codepoints, sorted.
static const font_glyph_t contiguous_glyphs[] = {
    {.codepoint = 'A', .x_advance = 1}, {.codepoint = 'B', .x_advance = 2},
    {.codepoint = 'C', .x_advance = 3}, {.codepoint = 'D', .x_advance = 4},
    {.codepoint = 'E', .x_advance = 5},
};
static const font_t font_contiguous = {
    .glyphs = contiguous_glyphs,
    .glyph_count = 5,
};

// Gap font: A, B, D, E — 'C' is intentionally absent.
static const font_glyph_t gap_glyphs[] = {
    {.codepoint = 'A', .x_advance = 1},
    {.codepoint = 'B', .x_advance = 2},
    {.codepoint = 'D', .x_advance = 4},
    {.codepoint = 'E', .x_advance = 5},
};
static const font_t font_gap = {
    .glyphs = gap_glyphs,
    .glyph_count = 4,
};

// Non-ASCII codepoint font: em-dash (U+2014).
static const font_glyph_t nonascii_glyphs[] = {
    {.codepoint = 0x2014, .x_advance = 12},
};
static const font_t font_nonascii = {
    .glyphs = nonascii_glyphs,
    .glyph_count = 1,
};

// --- Tests ---

static void test_empty_font_returns_null(void) {
    TEST_ASSERT_NULL(render_find_glyph(&font_empty, 'A'));
}

static void test_single_glyph_hit(void) {
    const font_glyph_t *g = render_find_glyph(&font_single, 'M');
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_HEX32('M', g->codepoint);
}

static void test_single_glyph_miss_below(void) {
    TEST_ASSERT_NULL(render_find_glyph(&font_single, 'A'));
}

static void test_single_glyph_miss_above(void) {
    TEST_ASSERT_NULL(render_find_glyph(&font_single, 'Z'));
}

static void test_multi_glyph_hit_first(void) {
    const font_glyph_t *g = render_find_glyph(&font_contiguous, 'A');
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_HEX32('A', g->codepoint);
}

static void test_multi_glyph_hit_last(void) {
    const font_glyph_t *g = render_find_glyph(&font_contiguous, 'E');
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_HEX32('E', g->codepoint);
}

static void test_multi_glyph_hit_middle(void) {
    const font_glyph_t *g = render_find_glyph(&font_contiguous, 'C');
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_HEX32('C', g->codepoint);
}

static void test_multi_glyph_miss_below_range(void) {
    TEST_ASSERT_NULL(render_find_glyph(&font_contiguous, '0'));
}

static void test_multi_glyph_miss_above_range(void) {
    TEST_ASSERT_NULL(render_find_glyph(&font_contiguous, 'Z'));
}

// Critical: codepoint falls in the gap between glyphs.
// A faulty binary search might return the nearest neighbour instead of NULL.
static void test_gap_in_middle_returns_null(void) {
    const font_glyph_t *g = render_find_glyph(&font_gap, 'C');
    TEST_ASSERT_NULL(g);
}

// Non-ASCII: catches accidental 8-bit truncation in the comparison.
static void test_non_ascii_codepoint_hit(void) {
    const font_glyph_t *g = render_find_glyph(&font_nonascii, 0x2014);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_HEX32(0x2014, g->codepoint);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_font_returns_null);
    RUN_TEST(test_single_glyph_hit);
    RUN_TEST(test_single_glyph_miss_below);
    RUN_TEST(test_single_glyph_miss_above);
    RUN_TEST(test_multi_glyph_hit_first);
    RUN_TEST(test_multi_glyph_hit_last);
    RUN_TEST(test_multi_glyph_hit_middle);
    RUN_TEST(test_multi_glyph_miss_below_range);
    RUN_TEST(test_multi_glyph_miss_above_range);
    RUN_TEST(test_gap_in_middle_returns_null);
    RUN_TEST(test_non_ascii_codepoint_hit);
    return UNITY_END();
}
