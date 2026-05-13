#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t codepoint;     // unicode codepoint this glyph represents
    uint32_t bitmap_offset; // index for bitmap_data array
    uint8_t width;          // glyph bitmap width in pixels
    uint8_t height;         // glyph bitmap height in pixels
    int8_t x_offset;        // x offset from cursor to bitmap left edge
    int8_t y_offset;        // y offset from baseline to bitmap top
    uint8_t x_advance;      // advance to next glyph
} font_glyph_t;

typedef struct {
    const char *name;    // fontname
    int16_t pixel_size;  // size font rendered at
    int16_t line_height; // line-to-line distance
    int16_t ascent;      // baseline to top
    int16_t descent;     // baseline to bottom
    uint32_t glyph_count;
    bool is_monospace; // hint for callers
    const font_glyph_t *glyphs;
    const uint8_t *bitmap_data;
    size_t bitmap_data_size;
} font_t;
