#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// returns final x position
int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text, size_t len);
void render_fill_rect(hal_framebuffer_t *fb, int x, int y, int w, int h, uint8_t value);
