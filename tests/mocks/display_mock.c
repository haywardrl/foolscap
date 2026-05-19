#include "display_mock.h"

#include "hal/hal_display.h"

#include <string.h>

// Mock framebuffer for tests. Real memory backing so tests can verify pixels
// if needed, but small enough to keep test binaries compact.
#define MOCK_FB_W 100
#define MOCK_FB_H 100

static uint8_t mock_pixels[MOCK_FB_W * MOCK_FB_H];
static hal_framebuffer_t mock_fb = {
    .pixels = mock_pixels,
    .width = MOCK_FB_W,
    .height = MOCK_FB_H,
    .stride = MOCK_FB_W,
};

int hal_display_init(void) {
    return 0;
}

void hal_display_shutdown(void) {
}

hal_framebuffer_t *hal_display_get_framebuffer(void) {
    return &mock_fb;
}

void hal_display_clear(uint8_t value) {
    memset(mock_pixels, value, sizeof(mock_pixels));
}

int hal_display_flush(void) {
    return 0;
}

void display_mock_reset(void) {
    memset(mock_pixels, 128, sizeof(mock_pixels));
}
