#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// returns final x position
int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text, size_t len);
void render_fill_rect(hal_framebuffer_t *fb, int x, int y, int w, int h, uint8_t value);

// cursor_byte clamped to len; writes screen coords to out_x/out_y
void render_compute_cursor_position(const font_t *font, int start_x, int baseline_y,
                                    const char *text, size_t len, size_t cursor_byte, int *out_x,
                                    int *out_y);
