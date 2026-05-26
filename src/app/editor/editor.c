#include "app/editor/editor.h"

#include "app/buffer/buffer.h"
#include "app/editor/editor_config.h"
#include "app/editor/editor_test.h"
#include "app/layout/layout.h"
#include "app/render/font.h"
#include "app/render/render.h"
#include "app/util/utf8.h"
#include "hal/hal_display.h"
#include "hal/hal_mem.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct editor {
    buffer_t *buf;
    char *scratch;
    layout_t layout;
    size_t scratch_capacity;
    const font_t *font;
    uint8_t cursor_width; // monospace assumed
    size_t preferred_col;
    bool content_dirty;
    bool cursor_dirty;
};

editor_t *editor_create(const font_t *font, size_t document_capacity) {
    if (font == NULL || document_capacity == 0) {
        return NULL;
    }
    if (font->glyph_count == 0) {
        return NULL; // font has no glyphs; can't determine cursor width
    }

    editor_t *ed = hal_mem_alloc(sizeof(*ed), HAL_MEM_DEFAULT);
    if (ed == NULL) {
        return NULL;
    }

    ed->buf = buffer_create(document_capacity);
    if (ed->buf == NULL) {
        hal_mem_free(ed);
        return NULL;
    }

    ed->scratch = hal_mem_alloc(document_capacity, HAL_MEM_LARGE);
    if (ed->scratch == NULL) {
        buffer_destroy(ed->buf);
        hal_mem_free(ed);
        return NULL;
    }

    ed->scratch_capacity = document_capacity;
    ed->layout = (layout_t){0};
    ed->font = font;
    ed->cursor_width = font->glyphs[0].x_advance;
    ed->preferred_col = SIZE_MAX; // unset
    ed->content_dirty = true;     // first render must build the initial (empty) layout
    ed->cursor_dirty = false;
    return ed;
}

void editor_destroy(editor_t *ed) {
    if (ed == NULL) {
        return;
    }
    hal_mem_free(ed->scratch);
    buffer_destroy(ed->buf);
    layout_destroy(&ed->layout);
    hal_mem_free(ed);
}

bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len) {
    bool changed = buffer_insert_bytes(ed->buf, bytes, len);
    if (changed) {
        ed->preferred_col = SIZE_MAX;
        ed->content_dirty = true;
        ed->cursor_dirty = true;
    }
    return changed;
}

bool editor_backspace(editor_t *ed) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    if (cursor_pos == 0) {
        return false;
    }
    size_t prev_boundary = buffer_prev_codepoint_boundary(ed->buf, cursor_pos);
    size_t bytes_to_delete = cursor_pos - prev_boundary;
    size_t deleted = buffer_delete_before_cursor(ed->buf, bytes_to_delete);
    if (deleted == 0) {
        return false;
    }
    ed->preferred_col = SIZE_MAX;
    ed->content_dirty = true;
    ed->cursor_dirty = true;
    return true;
}

bool editor_delete_forward(editor_t *ed) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    size_t buffer_len = buffer_size(ed->buf);
    if (cursor_pos == buffer_len) {
        return false;
    }
    size_t next_boundary = buffer_next_codepoint_boundary(ed->buf, cursor_pos);
    size_t bytes_to_delete = next_boundary - cursor_pos;
    size_t deleted = buffer_delete_after_cursor(ed->buf, bytes_to_delete);
    if (deleted == 0) {
        return false;
    }
    ed->preferred_col = SIZE_MAX;
    ed->content_dirty = true;
    ed->cursor_dirty = true;
    return true;
}

bool editor_move_cursor(editor_t *ed, editor_cursor_direction_t direction) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    size_t buffer_len = buffer_size(ed->buf);

    if (direction == EDITOR_CURSOR_LEFT || direction == EDITOR_CURSOR_RIGHT) {
        size_t target;
        if (direction == EDITOR_CURSOR_LEFT) {
            if (cursor_pos == 0)
                return false;
            target = buffer_prev_codepoint_boundary(ed->buf, cursor_pos);
        } else {
            if (cursor_pos == buffer_len)
                return false;
            target = buffer_next_codepoint_boundary(ed->buf, cursor_pos);
        }
        buffer_set_cursor(ed->buf, target);
        ed->preferred_col = SIZE_MAX;
        ed->cursor_dirty = true;
        return true;
    }

    // UP / DOWN: need a fresh layout against current buffer state
    size_t len = buffer_copy_contiguous(ed->buf, ed->scratch, ed->scratch_capacity);
    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    int wrap_width = fb->width - MARGIN_X;
    layout_destroy(&ed->layout);
    layout_compute(&ed->layout, ed->scratch, len, ed->font, wrap_width, MARGIN_X, MARGIN_TOP);

    size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_pos);

    if (direction == EDITOR_CURSOR_UP) {
        if (line_idx == 0)
            return false;
    } else { // EDITOR_CURSOR_DOWN
        if (line_idx + 1 >= ed->layout.count)
            return false;
    }

    const layout_line_t *cur = &ed->layout.lines[line_idx];
    size_t cur_col =
        utf8_count_codepoints(ed->scratch + cur->byte_start, cursor_pos - cur->byte_start);
    if (ed->preferred_col == SIZE_MAX)
        ed->preferred_col = cur_col;

    size_t target_idx = (direction == EDITOR_CURSOR_UP) ? line_idx - 1 : line_idx + 1;
    const layout_line_t *target_line = &ed->layout.lines[target_idx];
    size_t target_len = target_line->byte_end - target_line->byte_start;
    size_t target_cps = utf8_count_codepoints(ed->scratch + target_line->byte_start, target_len);
    size_t target_col = ed->preferred_col < target_cps ? ed->preferred_col : target_cps;

    size_t offset_in_line =
        utf8_advance_codepoints(ed->scratch + target_line->byte_start, target_len, target_col);
    buffer_set_cursor(ed->buf, target_line->byte_start + offset_in_line);
    ed->cursor_dirty = true;
    return true;
}

void editor_render(editor_t *ed) {
    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    if (ed->content_dirty) {
        size_t len = buffer_copy_contiguous(ed->buf, ed->scratch, ed->scratch_capacity);
        int wrap_width = fb->width - MARGIN_X; // right edge of text area
        layout_destroy(&ed->layout);
        layout_compute(&ed->layout, ed->scratch, len, ed->font, wrap_width, MARGIN_X, MARGIN_TOP);
    }

    render_fill_rect(fb, 0, 0, fb->width, fb->height, BACKGROUND);

    for (size_t i = 0; i < ed->layout.count; i++) {
        render_draw_string(fb, ed->font, MARGIN_X, ed->layout.lines[i].y,
                           ed->scratch + ed->layout.lines[i].byte_start,
                           ed->layout.lines[i].byte_end - ed->layout.lines[i].byte_start);
    }

    size_t cursor_byte = buffer_cursor_pos(ed->buf);
    size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_byte);
    const layout_line_t *line = &ed->layout.lines[line_idx];
    size_t cps_before_cursor =
        utf8_count_codepoints(ed->scratch + line->byte_start, cursor_byte - line->byte_start);
    int cursor_x = MARGIN_X + (int)cps_before_cursor * ed->cursor_width;
    int cursor_y = line->y;

    render_fill_rect(fb, cursor_x, cursor_y + CURSOR_Y_OFFSET, ed->cursor_width, CURSOR_HEIGHT,
                     FOREGROUND);

    ed->content_dirty = false;
    ed->cursor_dirty = false;
}

size_t editor_get_size(const editor_t *ed) {
    return buffer_size(ed->buf);
}

bool editor_is_dirty(const editor_t *ed) {
    return ed->content_dirty || ed->cursor_dirty;
}

const buffer_t *editor_test_get_buffer(const editor_t *ed) {
    return ed->buf;
}
