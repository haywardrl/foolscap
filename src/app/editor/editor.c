#include "app/editor/editor.h"

#include "app/buffer/buffer.h"
#include "app/editor/editor_config.h"
#include "app/editor/editor_test.h"
#include "app/render/font.h"
#include "app/render/render.h"
#include "hal/hal_display.h"
#include "hal/hal_mem.h"

#include <stdbool.h>
#include <stddef.h>

struct editor {
    buffer_t *buf;
    char *scratch;
    size_t scratch_capacity;
    const font_t *font;
    uint8_t cursor_width; // monospace assumed
    bool dirty;
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
    ed->font = font;
    ed->cursor_width = font->glyphs[0].x_advance;
    ed->dirty = false;
    return ed;
}

void editor_destroy(editor_t *ed) {
    if (ed == NULL) {
        return;
    }
    hal_mem_free(ed->scratch);
    buffer_destroy(ed->buf);
    hal_mem_free(ed);
}

bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len) {
    bool changed = buffer_insert_bytes(ed->buf, bytes, len);
    if (changed) {
        ed->dirty = true;
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
    ed->dirty = true;
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
    ed->dirty = true;
    return true;
}

bool editor_move_cursor(editor_t *ed, editor_cursor_direction_t direction) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    size_t buffer_len = buffer_size(ed->buf);

    size_t target;
    if (direction == EDITOR_CURSOR_LEFT) {
        if (cursor_pos == 0)
            return false;
        target = buffer_prev_codepoint_boundary(ed->buf, cursor_pos);
    } else { // EDITOR_CURSOR_RIGHT
        if (cursor_pos == buffer_len)
            return false;
        target = buffer_next_codepoint_boundary(ed->buf, cursor_pos);
    }

    buffer_set_cursor(ed->buf, target);
    ed->dirty = true;
    return true;
}

void editor_render(editor_t *ed) {
    hal_framebuffer_t *fb = hal_display_get_framebuffer();

    hal_display_clear(BACKGROUND);

    size_t len = buffer_copy_contiguous(ed->buf, ed->scratch, ed->scratch_capacity);
    render_draw_string(fb, ed->font, MARGIN_X, MARGIN_TOP, ed->scratch, len);

    int cursor_x, cursor_y;
    size_t cursor_byte = buffer_cursor_pos(ed->buf);
    render_compute_cursor_position(ed->font, MARGIN_X, MARGIN_TOP, ed->scratch, len, cursor_byte,
                                   &cursor_x, &cursor_y);

    render_fill_rect(fb, cursor_x, cursor_y + CURSOR_Y_OFFSET, ed->cursor_width, CURSOR_HEIGHT,
                     FOREGROUND);

    ed->dirty = false;
}

size_t editor_get_size(const editor_t *ed) {
    return buffer_size(ed->buf);
}

bool editor_is_dirty(const editor_t *ed) {
    return ed->dirty;
}

const buffer_t *editor_test_get_buffer(const editor_t *ed) {
    return ed->buf;
}
