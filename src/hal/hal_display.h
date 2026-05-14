#pragma once
#include <stdint.h>

// Display HAL: single-threaded. All functions called from the same thread.
// Pixel format: 8-bit grayscale. 0 = black ink, 255 = white paper.
// Dimensions are fixed between init and shutdown.

typedef struct {
    uint8_t *pixels; // row-major, row i starts at pixels + i * stride
    int width;       // pixels per row
    int height;      // rows
    int stride;      // bytes per row (>= width; ports may align)
} hal_framebuffer_t;

// Lifecycle
int hal_display_init(void);
void hal_display_shutdown(void);

// Get the framebuffer to write pixels into. Caller must not free.
hal_framebuffer_t *hal_display_get_framebuffer(void);

// Fill the entire framebuffer with a single grayscale value.
void hal_display_clear(uint8_t value);

// Push framebuffer contents to the physical display.
int hal_display_flush(void);
