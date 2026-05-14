#pragma once

#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

void render_clear(hal_framebuffer_t *fb, uint8_t value);

// Draws UTF-8 text. baseline_y is the y position of the text baseline
// (i.e. where the bottom of an 'A' sits; descenders go below this).
// Returns the final cursor x position after drawing.
int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text);
