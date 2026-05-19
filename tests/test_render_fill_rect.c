#include "app/render/render.h"
#include "hal/hal_display.h"

#include <stdint.h>
#include <string.h>
#include <unity.h>

#define FB_W 10
#define FB_H 10
static uint8_t fb_pixels[FB_W * FB_H];
static hal_framebuffer_t fb;

// Strided framebuffer: width=10 but stride=16 (simulates aligned rows).
#define STRIDE_W 10
#define STRIDE_H 10
#define STRIDE_BYTES 16
static uint8_t strided_pixels[STRIDE_BYTES * STRIDE_H];
static hal_framebuffer_t strided_fb;

void setUp(void) {
    memset(fb_pixels, 128, sizeof(fb_pixels));
    fb.pixels = fb_pixels;
    fb.width = FB_W;
    fb.height = FB_H;
    fb.stride = FB_W;

    memset(strided_pixels, 128, sizeof(strided_pixels));
    strided_fb.pixels = strided_pixels;
    strided_fb.width = STRIDE_W;
    strided_fb.height = STRIDE_H;
    strided_fb.stride = STRIDE_BYTES;
}

void tearDown(void) {
}

static void test_fill_framebuffer(void) {
    render_fill_rect(&fb, 0, 0, FB_W, FB_H, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[0]);
}

static void test_fill_mid_screen_rect(void) {
    render_fill_rect(&fb, 5, 5, 5, 5, 0);
    // Pixel inside the rect (7, 7)
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[7 * FB_W + 7]);
    // Pixels outside the rect, untouched
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[0]);            // (0,0) top-left
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[2 * FB_W + 2]); // (2,2) above-left of rect
}

static void test_negative_x_clipped(void) {
    // Pixel inside the rect (-2, 3)
    render_fill_rect(&fb, -2, 3, 5, 5, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[3 * FB_W + 2]);
    // Pixels outside the rect, untouched
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[0]);            // (0,0) top-left
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[3 * FB_W + 5]); // (0,0) top-left
}

static void test_negative_y_clipped(void) {
    // Pixel inside the rect (-2, 3)
    render_fill_rect(&fb, 2, -3, 5, 5, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[0 * FB_W + 2]);
    // Pixels outside the rect, untouched
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[0]);            // (0,0) top-left
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[6 * FB_W + 5]); // (0,0) top-left
}

static void test_right_edge_clipped(void) {
    // Rect at (8, 3) width 5 extends to x=12, framebuffer is only 10 wide.
    // Clipping: w = 10 - 8 = 2. Final fill at (8,3), w=2, h=2.
    render_fill_rect(&fb, 8, 3, 5, 2, 0);
    // Last column of the framebuffer is the boundary case — must be filled.
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[3 * FB_W + 8]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[3 * FB_W + 9]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[4 * FB_W + 9]);
    // Outside the clipped rect
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[3 * FB_W + 7]); // (7,3) left of rect
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[5 * FB_W + 9]); // (9,5) below rect
}

static void test_bottom_edge_clipped(void) {
    // Rect at (3, 8) height 5 extends to y=13, framebuffer is only 10 tall.
    // Clipping: h = 10 - 8 = 2. Final fill at (3,8), w=2, h=2.
    render_fill_rect(&fb, 3, 8, 2, 5, 0);
    // Last row of the framebuffer is the boundary case — must be filled.
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[8 * FB_W + 3]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[9 * FB_W + 3]);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[9 * FB_W + 4]);
    // Outside the clipped rect
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[7 * FB_W + 3]); // (3,7) above rect
    TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[9 * FB_W + 5]); // (5,9) right of rect
}

static void test_zero_width_is_noop(void) {
    render_fill_rect(&fb, 5, 5, 0, 5, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_zero_height_is_noop(void) {
    render_fill_rect(&fb, 5, 5, 5, 0, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_stride_respected(void) {
    // Fill a 2x2 rect at (0, 1). Stride is 16, width is 10.
    // Row 1 starts at byte 16 (not byte 10).
    render_fill_rect(&strided_fb, 0, 1, 2, 2, 0);
    // (0,1) → byte 16. (1,1) → byte 17.
    TEST_ASSERT_EQUAL_UINT8(0, strided_pixels[16]);
    TEST_ASSERT_EQUAL_UINT8(0, strided_pixels[17]);
    // (0,2) → byte 32. (1,2) → byte 33.
    TEST_ASSERT_EQUAL_UINT8(0, strided_pixels[32]);
    TEST_ASSERT_EQUAL_UINT8(0, strided_pixels[33]);
    // Padding bytes between rows must be untouched.
    // Bytes 10-15 are after row 0's pixels but before row 1's start.
    TEST_ASSERT_EQUAL_UINT8(128, strided_pixels[10]);
    TEST_ASSERT_EQUAL_UINT8(128, strided_pixels[15]);
    // Outside the filled rect on row 1
    TEST_ASSERT_EQUAL_UINT8(128, strided_pixels[16 + 2]); // (2, 1)
}

static void test_entirely_out_of_bounds_is_noop(void) {
    // Rect far past the framebuffer in both axes.
    render_fill_rect(&fb, 100, 100, 50, 50, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
    // Also test fully-negative origin (after clipping, w/h go negative).
    render_fill_rect(&fb, -100, -100, 50, 50, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fill_framebuffer);
    RUN_TEST(test_fill_mid_screen_rect);
    RUN_TEST(test_negative_x_clipped);
    RUN_TEST(test_negative_y_clipped);
    RUN_TEST(test_right_edge_clipped);
    RUN_TEST(test_bottom_edge_clipped);
    RUN_TEST(test_zero_width_is_noop);
    RUN_TEST(test_zero_height_is_noop);
    RUN_TEST(test_stride_respected);
    RUN_TEST(test_entirely_out_of_bounds_is_noop);
    return UNITY_END();
}
