#pragma once
#include <stdint.h>

// 8-bit grayscale; single-threaded

typedef struct {
    uint8_t *pixels; // row-major, row i starts at pixels + i * stride
    int width;       // pixels per row
    int height;      // rows
    int stride;      // bytes per row (>= width; ports may align)
} hal_framebuffer_t;

// Lifecycle
int hal_display_init(void);
void hal_display_shutdown(void);

// caller must not free
hal_framebuffer_t *hal_display_get_framebuffer(void);

void hal_display_clear(uint8_t value);
int hal_display_full_flush(void);
int hal_display_flush_region(int x, int y, int w, int h);
