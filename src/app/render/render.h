#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// Draws UTF-8 text into the framebuffer.
// baseline_y is the y position of the text baseline (descenders go below).
// Returns the final cursor x position after drawing.
int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text, size_t len);
// Used to draw the cursor for now
void render_fill_rect(hal_framebuffer_t *fb, int x, int y, int w, int h, uint8_t value);
