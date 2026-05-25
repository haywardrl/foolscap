#include "layout.h"

#include "app/util/utf8.h"
#include "hal/hal_mem.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int layout_lines_append(layout_t *out, const layout_line_t *line);

bool layout_compute(layout_t *out, const char *text, size_t len, const font_t *font, int wrap_width,
                    int margin_x, int margin_top) {
    out->lines = NULL;
    out->count = 0;
    out->capacity = 0;

    utf8_iter_t it;
    utf8_iter_init(&it, text, len);

    int x = margin_x;
    int y = margin_top;
    size_t line_start = 0;
    size_t last_space_byte = SIZE_MAX;

    uint32_t codepoint;
    utf8_status_t status;
    // Monospace font used for now, all x_advance are the same
    int x_advance = font->glyphs[0].x_advance;

    while (1) {
        size_t byte_before = it.p - text;
        status = utf8_next(&it, &codepoint);
        if (status == UTF8_END)
            break;
        if (status == UTF8_INVALID)
            continue;
        size_t byte_after = it.p - text;

        if (codepoint == '\n') {
            // byte_end excludes the \n itself; line_start skips past it
            layout_line_t line = {.byte_start = line_start,
                                  .byte_end = byte_before,
                                  .y = y,
                                  .ends_with_newline = true};
            if (layout_lines_append(out, &line) != 0)
                goto fail;
            line_start = byte_after;
            x = margin_x;
            y += font->line_height;
            last_space_byte = SIZE_MAX;
        } else {
            if (codepoint == ' ')
                last_space_byte = byte_before;

            x += x_advance;

            if (x > wrap_width) {
                if (last_space_byte != SIZE_MAX) {
                    layout_line_t line = {.byte_start = line_start,
                                          .byte_end = last_space_byte,
                                          .y = y,
                                          .ends_with_newline = false};
                    if (layout_lines_append(out, &line) != 0)
                        goto fail;
                    line_start = last_space_byte + 1;
                    size_t cps = utf8_count_codepoints(text + line_start, byte_after - line_start);
                    x = margin_x + (int)cps * x_advance;
                } else {
                    layout_line_t line = {.byte_start = line_start,
                                          .byte_end = byte_before,
                                          .y = y,
                                          .ends_with_newline = false};
                    if (layout_lines_append(out, &line) != 0)
                        goto fail;
                    line_start = byte_before;
                    x = margin_x + x_advance;
                }
                y += font->line_height;
                last_space_byte = SIZE_MAX;
            }
        }
    }

    // emit final line
    layout_line_t line = {
        .byte_start = line_start, .byte_end = len, .y = y, .ends_with_newline = false};
    if (layout_lines_append(out, &line) != 0)
        goto fail;

    return true;

fail:
    layout_destroy(out);
    return false;
}

void layout_destroy(layout_t *out) {
    if (out->lines)
        hal_mem_free(out->lines);
    out->lines = NULL;
    out->count = 0;
    out->capacity = 0;
}

static int layout_lines_append(layout_t *out, const layout_line_t *line) {
    if (out->count + 1 > out->capacity) {
        size_t new_capacity = out->capacity ? out->capacity * 2 : 16;
        layout_line_t *grown = hal_mem_alloc(new_capacity * sizeof(layout_line_t), HAL_MEM_DEFAULT);
        if (!grown)
            return -1;
        if (out->lines) {
            memcpy(grown, out->lines, out->count * sizeof(layout_line_t));
            hal_mem_free(out->lines);
        }
        out->lines = grown;
        out->capacity = new_capacity;
    }
    out->lines[out->count++] = *line;
    return 0;
}

size_t layout_find_line_for_byte(const layout_t *out, size_t byte) {
    for (size_t i = out->count; i > 0; i--) {
        if (out->lines[i - 1].byte_start <= byte) {
            return i - 1;
        }
    }
    return 0;
}
