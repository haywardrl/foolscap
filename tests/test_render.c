#include "app/render/render.h"
#include "app/render/render_internal.h" // if needed for direct access
#include "hal/hal_display.h"
#include "unity_internals.h"

#include <stdint.h>
#include <string.h>
#include <unity.h>

// Framebuffer fixture — reset in setUp
#define FB_W 40
#define FB_H 40
static uint8_t fb_pixels[FB_W * FB_H];
static hal_framebuffer_t test_fb;
// Used for initial cursor location
#define START_X 10
#define BASELINE_Y 30

// Cursor-only font: zero-width glyphs, different x_advance values.
// Used for tests that only care about cursor arithmetic, not pixels.
// Different advances (10 vs 7) ensure cumulative tests can't pass by accident.
static const font_glyph_t cursor_glyphs[] = {
    {.codepoint = 'A',
     .bitmap_offset = 0,
     .width = 0,
     .height = 0,
     .x_offset = 0,
     .y_offset = 0,
     .x_advance = 10},
    {.codepoint = 'B',
     .bitmap_offset = 0,
     .width = 0,
     .height = 0,
     .x_offset = 0,
     .y_offset = 0,
     .x_advance = 7},
};
static const font_t cursor_font = {
    .glyphs = cursor_glyphs,
    .glyph_count = sizeof(cursor_glyphs) / sizeof(cursor_glyphs[0]),
    .line_height = 20,
    .bitmap_data = NULL,
    .bitmap_data_size = 0,
};

// Integration: real bitmap glyph proves the full pipeline draws pixels.
// Hand-built 3x3 filled glyph for 'X'. Bitmap is 0xE0, 0xE0, 0xE0 — top 3
// bits set in each of 3 rows = a 3x3 solid block.
static const uint8_t bitmap_3x3_filled[] = {0xE0, 0xE0, 0xE0};
static const font_glyph_t bitmap_glyphs[] = {
    {.codepoint = 'X',
     .bitmap_offset = 0,
     .width = 3,
     .height = 3,
     .x_offset = 0,
     .y_offset = 0,
     .x_advance = 5},
};
static const font_t bitmap_font = {
    .glyphs = bitmap_glyphs,
    .glyph_count = 1,
    .line_height = 10,
    .bitmap_data = bitmap_3x3_filled,
    .bitmap_data_size = sizeof(bitmap_3x3_filled),
};

void setUp(void) {
    memset(fb_pixels, 128, sizeof(fb_pixels));
    test_fb.pixels = fb_pixels;
    test_fb.width = FB_W;
    test_fb.height = FB_H;
    test_fb.stride = FB_W;
}

void tearDown(void) {
}

// Helper: assert no pixel in the framebuffer has been touched.
// Useful for empty-string and newline-only tests.
static void assert_fb_unchanged(void) {
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_single_char_advances_cursor(void) {
    int advance = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "A", 1, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + cursor_glyphs[0].x_advance, advance);
    assert_fb_unchanged();
}

static void test_multi_char_cumulative_advance(void) {
    int advance = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "AB", 2, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + cursor_glyphs[0].x_advance + cursor_glyphs[1].x_advance, advance);
    assert_fb_unchanged();
}

static void test_empty_string_returns_start_x(void) {
    int advance = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "", 0, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X, advance);
    assert_fb_unchanged();
}

static void test_newline_resets_x(void) {
    int advance_x = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "A\n", 2, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X, advance_x);
    assert_fb_unchanged();
}

static void test_newline_only_returns_start_x(void) {
    int advance_x = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "\n", 1, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X, advance_x);
    assert_fb_unchanged();
}

static void test_newline_plus_char_advances_x(void) {
    int advance_x = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "A\nB", 3, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + cursor_glyphs[1].x_advance, advance_x);
    assert_fb_unchanged();
}

static void test_missing_codepoint_does_not_advance(void) {
    int advance_x = render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, "AZA", 3, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + 2 * cursor_glyphs[0].x_advance, advance_x);
    assert_fb_unchanged();
}

static void test_invalid_utf8_does_not_advance(void) {
    // 0xFF is invalid UTF-8 lead. utf8_next should report INVALID,
    // draw_string should `continue` past it. Then 'A' renders normally.
    const char input[] = {(char)0xFF, 'A'};
    int advance_x =
        render_draw_string(&test_fb, &cursor_font, START_X, BASELINE_Y, input, sizeof(input), (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + cursor_glyphs[0].x_advance, advance_x);
    assert_fb_unchanged();
}

static void test_bitmap_glyph_renders_pixels(void) {
    int advance_x = render_draw_string(&test_fb, &bitmap_font, START_X, BASELINE_Y, "X", 1, (rect_t){0});
    TEST_ASSERT_EQUAL(START_X + bitmap_glyphs[0].x_advance, advance_x);
    // Expect a 3x3 block of zero pixels at (START_X, BASELINE_Y).
    // (y_offset and x_offset are zero, so glyph origin == cursor position.)
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int idx = (BASELINE_Y + row) * FB_W + (START_X + col);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, fb_pixels[idx], "glyph pixel not drawn");
        }
    }
    // Pixel just outside the glyph should be untouched (= 128).
    int outside = BASELINE_Y * FB_W + (START_X + 3);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(128, fb_pixels[outside], "wrote outside glyph bounds");
}

static void test_clip_skips_pixels_outside(void) {
    // 3x3 glyph at (START_X, BASELINE_Y); clip to its top-left 2x2 corner.
    rect_t clip = {START_X, BASELINE_Y, 2, 2};
    render_draw_string(&test_fb, &bitmap_font, START_X, BASELINE_Y, "X", 1, clip);
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL_MESSAGE(4, set_count, "clip should keep only the 2x2 corner");
    int outside = BASELINE_Y * FB_W + (START_X + 2);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(128, fb_pixels[outside], "pixel outside clip was drawn");
}

static void test_empty_clip_draws_everything(void) {
    // an empty clip means no clipping: all 9 pixels draw.
    render_draw_string(&test_fb, &bitmap_font, START_X, BASELINE_Y, "X", 1, (rect_t){0});
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL(9, set_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clip_skips_pixels_outside);
    RUN_TEST(test_empty_clip_draws_everything);
    RUN_TEST(test_single_char_advances_cursor);
    RUN_TEST(test_multi_char_cumulative_advance);
    RUN_TEST(test_empty_string_returns_start_x);
    RUN_TEST(test_newline_resets_x);
    RUN_TEST(test_newline_only_returns_start_x);
    RUN_TEST(test_newline_plus_char_advances_x);
    RUN_TEST(test_missing_codepoint_does_not_advance);
    RUN_TEST(test_invalid_utf8_does_not_advance);
    RUN_TEST(test_bitmap_glyph_renders_pixels);
    return UNITY_END();
}
