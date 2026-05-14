#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// Binary search for `codepoint` in `font->glyphs`.
// Returns the glyph or NULL if not present. O(log n).
const font_glyph_t *render_find_glyph(const font_t *font, uint32_t codepoint);

// Writes `value` to pixel (x, y) if in bounds. Clips silently otherwise.
void render_put_pixel(hal_framebuffer_t *fb, int x, int y, uint8_t value);

// Rasterizes a single glyph into the framebuffer.
// Returns the glyph's x_advance (caller adds to cursor).
int render_draw_glyph(hal_framebuffer_t *fb, const font_t *font, const font_glyph_t *glyph, int x,
                      int baseline_y);
