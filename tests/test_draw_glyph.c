#include "app/render/render.h"
#include "app/render/render_internal.h"
#include "hal/hal_display.h"

#include <stdint.h>
#include <string.h>
#include <unity.h>

#define FB_W 20
#define FB_H 20
static uint8_t fb_pixels[FB_W * FB_H];
static hal_framebuffer_t fb;

void setUp(void) {
    memset(fb_pixels, 128, sizeof(fb_pixels));
    fb.pixels = fb_pixels;
    fb.width = FB_W;
    fb.height = FB_H;
    fb.stride = FB_W;
}

void tearDown(void) {
}

// --- Fixtures ---

// 3x3 solid filled glyph: top 3 bits of each row set.
static const uint8_t bitmap_3x3_filled[] = {0xE0, 0xE0, 0xE0};
static const font_glyph_t glyph_3x3_filled = {
    .codepoint = 'X',
    .bitmap_offset = 0,
    .width = 3,
    .height = 3,
    .x_offset = 0,
    .y_offset = 0,
    .x_advance = 5,
};
static const font_t font_3x3 = {
    .glyphs = &glyph_3x3_filled,
    .glyph_count = 1,
    .line_height = 5,
    .bitmap_data = bitmap_3x3_filled,
    .bitmap_data_size = sizeof(bitmap_3x3_filled),
};

// 1x1 single-pixel glyph.
static const uint8_t bitmap_1x1[] = {0x80};
static const font_glyph_t glyph_1x1 = {
    .codepoint = '.',
    .bitmap_offset = 0,
    .width = 1,
    .height = 1,
    .x_offset = 0,
    .y_offset = 0,
    .x_advance = 2,
};
static const font_t font_1x1 = {
    .glyphs = &glyph_1x1,
    .glyph_count = 1,
    .line_height = 2,
    .bitmap_data = bitmap_1x1,
    .bitmap_data_size = sizeof(bitmap_1x1),
};

// 3x3 diagonal: bit pattern picks out one pixel per row at a different column.
// Row 0: 0x80 = 10000000 → col 0 set
// Row 1: 0x40 = 01000000 → col 1 set
// Row 2: 0x20 = 00100000 → col 2 set
static const uint8_t bitmap_3x3_diag[] = {0x80, 0x40, 0x20};
static const font_glyph_t glyph_3x3_diag = {
    .codepoint = '\\',
    .bitmap_offset = 0,
    .width = 3,
    .height = 3,
    .x_offset = 0,
    .y_offset = 0,
    .x_advance = 5,
};
static const font_t font_3x3_diag = {
    .glyphs = &glyph_3x3_diag,
    .glyph_count = 1,
    .line_height = 5,
    .bitmap_data = bitmap_3x3_diag,
    .bitmap_data_size = sizeof(bitmap_3x3_diag),
};

// Glyph with non-zero offsets.
static const font_glyph_t glyph_offset = {
    .codepoint = 'O',
    .bitmap_offset = 0,
    .width = 1,
    .height = 1,
    .x_offset = 2,
    .y_offset = 3,
    .x_advance = 4,
};
static const font_t font_offset = {
    .glyphs = &glyph_offset,
    .glyph_count = 1,
    .line_height = 5,
    .bitmap_data = bitmap_1x1,
    .bitmap_data_size = sizeof(bitmap_1x1),
};

// 11-wide glyph: 11 set bits across 2 bytes. Row stride = (11+7)/8 = 2.
// Row pattern: 0xFF, 0xE0 = 11111111 11100000 → bits 0..10 set, bits 11..15 padding.
// A buggy draw_glyph that iterates col < 16 instead of col < width would
// draw 5 extra pixels into the padding zone.
static const uint8_t bitmap_11w[] = {0xFF, 0xE0};
static const font_glyph_t glyph_11w = {
    .codepoint = 'W',
    .bitmap_offset = 0,
    .width = 11,
    .height = 1,
    .x_offset = 0,
    .y_offset = 0,
    .x_advance = 12,
};
static const font_t font_11w = {
    .glyphs = &glyph_11w,
    .glyph_count = 1,
    .line_height = 2,
    .bitmap_data = bitmap_11w,
    .bitmap_data_size = sizeof(bitmap_11w),
};

// --- Tests ---

static void test_3x3_filled_draws_nine_pixels(void) {
    int adv = render_draw_glyph(&fb, &font_3x3, &glyph_3x3_filled, 5, 5);
    TEST_ASSERT_EQUAL(glyph_3x3_filled.x_advance, adv);
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int idx = (5 + row) * FB_W + (5 + col);
            TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[idx]);
        }
    }
}

static void test_1x1_draws_exactly_one_pixel(void) {
    render_draw_glyph(&fb, &font_1x1, &glyph_1x1, 5, 5);
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL(1, set_count);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[5 * FB_W + 5]);
}

static void test_3x3_diagonal_set_bits_only(void) {
    render_draw_glyph(&fb, &font_3x3_diag, &glyph_3x3_diag, 5, 5);
    // Expect exactly (5,5), (6,6), (7,7) to be 0; nothing else.
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL(3, set_count);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[5 * FB_W + 5]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[6 * FB_W + 6]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[7 * FB_W + 7]);
}

static void test_x_and_y_offset_applied(void) {
    // Draw at (5, 5) but glyph has x_offset=2, y_offset=3.
    // The single pixel should land at (7, 8).
    render_draw_glyph(&fb, &font_offset, &glyph_offset, 5, 5);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[(5 + 3) * FB_W + (5 + 2)]);
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL(1, set_count);
}

static void test_edge_clipping_no_crash(void) {
    // Draw 3x3 glyph at x = FB_W - 2: column 2 of the glyph would land at FB_W,
    // which is out of bounds. put_pixel should clip; no crash.
    int adv = render_draw_glyph(&fb, &font_3x3, &glyph_3x3_filled, FB_W - 2, 5);
    TEST_ASSERT_EQUAL(glyph_3x3_filled.x_advance, adv);
    // Two in-bounds columns × 3 rows = 6 pixels expected drawn.
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL(6, set_count);
}

static void test_returns_x_advance(void) {
    int adv = render_draw_glyph(&fb, &font_3x3, &glyph_3x3_filled, 5, 5);
    TEST_ASSERT_EQUAL(glyph_3x3_filled.x_advance, adv);
}

// 11-wide glyph: 11 set bits, 5 padding bits in the second byte.
// Verifies the inner loop iterates col < width (not col < bytes*8).
static void test_11_wide_glyph_no_padding_leak(void) {
    render_draw_glyph(&fb, &font_11w, &glyph_11w, 0, 5);
    int set_count = 0;
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        if (fb_pixels[i] == 0)
            set_count++;
    }
    TEST_ASSERT_EQUAL_MESSAGE(11, set_count, "padding bits leaked into framebuffer");
    // Check pixel at col 10 (last valid) is set, col 11 (first padding) is not.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, fb_pixels[5 * FB_W + 10], "last valid bit not drawn");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(128, fb_pixels[5 * FB_W + 11], "padding bit drew a pixel");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_3x3_filled_draws_nine_pixels);
    RUN_TEST(test_1x1_draws_exactly_one_pixel);
    RUN_TEST(test_3x3_diagonal_set_bits_only);
    RUN_TEST(test_x_and_y_offset_applied);
    RUN_TEST(test_edge_clipping_no_crash);
    RUN_TEST(test_returns_x_advance);
    RUN_TEST(test_11_wide_glyph_no_padding_leak);
    return UNITY_END();
}
