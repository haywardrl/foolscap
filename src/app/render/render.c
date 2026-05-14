#include "render.h"

#include "app/util/utf8.h"
#include "font.h"
#include "render_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

const font_glyph_t *render_find_glyph(const font_t *font, uint32_t codepoint) {
    if (font->glyph_count == 0) {
        return NULL;
    }
    int low = 0;
    int high = (int)font->glyph_count - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        uint32_t cp = font->glyphs[mid].codepoint;
        if (cp < codepoint) {
            low = mid + 1;
        } else if (cp > codepoint) {
            high = mid - 1;
        } else {
            return &font->glyphs[mid];
        }
    }
    return NULL;
}

void render_put_pixel(hal_framebuffer_t *fb, int x, int y, uint8_t value) {
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) {
        return;
    }
    fb->pixels[y * fb->stride + x] = value;
}

int render_draw_glyph(hal_framebuffer_t *fb, const font_t *font, const font_glyph_t *glyph, int x,
                      int baseline_y) {
    int row_stride = (glyph->width + 7) / 8;
    for (int row = 0; row < glyph->height; row++) {
        for (int col = 0; col < glyph->width; col++) {
            int byte_index = glyph->bitmap_offset + row * row_stride + (col / 8);
            uint8_t byte = font->bitmap_data[byte_index];
            int bit_index = 7 - (col % 8);
            int bit = (byte >> bit_index) & 1;
            if (bit == 1) {
                render_put_pixel(fb, x + glyph->x_offset + col, baseline_y + glyph->y_offset + row,
                                 0);
            }
        }
    }
    return glyph->x_advance;
}

int render_draw_string(hal_framebuffer_t *fb, const font_t *font, int x, int baseline_y,
                       const char *utf8_text) {
    int start_x = x;
    int cursor_x = x;
    int cursor_y = baseline_y;
    utf8_iter_t it;
    utf8_iter_init(&it, utf8_text, strlen(utf8_text));

    uint32_t codepoint = 0;
    utf8_status_t status = UTF8_OK;
    while ((status = utf8_next(&it, &codepoint)) != UTF8_END) {
        if (status == UTF8_INVALID) {
            continue;
        }
        if (codepoint == '\n') {
            cursor_x = start_x;
            cursor_y += font->line_height;
            continue;
        }
        const font_glyph_t *glyph = render_find_glyph(font, codepoint);
        if (glyph == NULL) {
            continue;
        }
        cursor_x += render_draw_glyph(fb, font, glyph, cursor_x, cursor_y);
    }
    return cursor_x;
}
