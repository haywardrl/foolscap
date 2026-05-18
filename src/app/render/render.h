#pragma once
#include "font.h"
#include "hal/hal_display.h"

#include <stdint.h>

// Draws UTF-8 text into the framebuffer.
// baseline_y is the y position of the text baseline (descenders go below).
// Returns the final cursor x position after drawing.
int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text, size_t len);
// Fill a rectangle with `value`. Clips to framebuffer bounds.
// Used for background clear, cursor underline, future UI.
void render_fill_rect(hal_framebuffer_t *fb, int x, int y, int w, int h, uint8_t value);

// Compute where the cursor would land if `text` were drawn at (start_x, baseline_y)
// and the cursor sits at byte position `cursor_byte` in the text.
//
// Writes the resulting screen position to *out_x and *out_y.
//
// Contract:
// - cursor_byte must be a UTF-8 codepoint boundary in `text` (caller's responsibility)
// - cursor_byte may equal `len` (cursor at end-of-text)
// - cursor_byte > len is clamped to len
//
// This function does not draw anything; it only computes position.
void render_compute_cursor_position(const font_t *font, int start_x, int baseline_y,
                                    const char *text, size_t len, size_t cursor_byte, int *out_x,
                                    int *out_y);
