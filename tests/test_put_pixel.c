#include "app/render/render_internal.h"
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

static void test_write_origin(void) {
    render_put_pixel(&fb, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[0]);
}

static void test_write_middle(void) {
    render_put_pixel(&fb, 5, 5, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[5 * FB_W + 5]);
}

static void test_write_bottom_right_corner(void) {
    render_put_pixel(&fb, FB_W - 1, FB_H - 1, 0);
    TEST_ASSERT_EQUAL_UINT8(0, fb_pixels[(FB_H - 1) * FB_W + (FB_W - 1)]);
}

static void test_no_op_x_negative(void) {
    render_put_pixel(&fb, -1, 5, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_no_op_x_equals_width(void) {
    render_put_pixel(&fb, FB_W, 5, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_no_op_y_negative(void) {
    render_put_pixel(&fb, 5, -1, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_no_op_y_equals_height(void) {
    render_put_pixel(&fb, 5, FB_H, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

static void test_no_op_extreme_out_of_range(void) {
    render_put_pixel(&fb, 99999, 99999, 0);
    render_put_pixel(&fb, -99999, -99999, 0);
    for (size_t i = 0; i < sizeof(fb_pixels); i++) {
        TEST_ASSERT_EQUAL_UINT8(128, fb_pixels[i]);
    }
}

// CRITICAL: stride is the byte distance between rows, which can exceed width.
// On ESP32 with aligned framebuffers, stride > width is realistic.
// A buggy put_pixel using width instead of stride writes to the wrong byte.
static void test_stride_respected(void) {
    // Write at (0, 1). With stride=16, this should land at byte offset 16.
    // A buggy put_pixel using width would write to byte 10.
    render_put_pixel(&strided_fb, 0, 1, 0);

    // Exactly one byte should be 0, and it should be at byte offset 16.
    int zero_count = 0;
    int zero_idx = -1;
    for (size_t i = 0; i < sizeof(strided_pixels); i++) {
        if (strided_pixels[i] == 0) {
            zero_count++;
            zero_idx = (int)i;
        }
    }
    TEST_ASSERT_EQUAL_MESSAGE(1, zero_count, "expected exactly one pixel written");
    TEST_ASSERT_EQUAL_MESSAGE(STRIDE_BYTES, zero_idx,
                              "pixel landed at wrong offset (likely used width not stride)");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_origin);
    RUN_TEST(test_write_middle);
    RUN_TEST(test_write_bottom_right_corner);
    RUN_TEST(test_no_op_x_negative);
    RUN_TEST(test_no_op_x_equals_width);
    RUN_TEST(test_no_op_y_negative);
    RUN_TEST(test_no_op_y_equals_height);
    RUN_TEST(test_no_op_extreme_out_of_range);
    RUN_TEST(test_stride_respected);
    return UNITY_END();
}
