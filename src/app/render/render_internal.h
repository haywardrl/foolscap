#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// binary search; NULL if not found
const font_glyph_t *render_find_glyph(const font_t *font, uint32_t codepoint);

// writes to (x,y); clips silently
void render_put_pixel(hal_framebuffer_t *fb, int x, int y, uint8_t value);

// returns x_advance
int render_draw_glyph(hal_framebuffer_t *fb, const font_t *font, const font_glyph_t *glyph, int x,
                      int baseline_y);
