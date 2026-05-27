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
    char *line_scratch; // straddle-copy space for one line, not the document
    layout_t layout;
    size_t line_scratch_capacity;
    const font_t *font;
    uint8_t cursor_width; // monospace assumed
    size_t preferred_col;
    damage_t damage;         // pixels to repaint, coalesced across this frame
    rect_t last_cursor_rect; // cell painted last render, erased on the next move
};

// the cell occupied by the cursor at (cursor_x, baseline), the glyph box plus
// the underline below it, so erasing it never clips a neighbour's descender.
static rect_t editor_cursor_rect(const editor_t *ed, int cursor_x, int baseline_y) {
    int top = baseline_y - ed->font->ascent;
    int bottom = baseline_y + CURSOR_Y_OFFSET + CURSOR_HEIGHT;
    return (rect_t){cursor_x, top, ed->cursor_width, bottom - top};
}

static void editor_mark_full(editor_t *ed) {
    const hal_framebuffer_t *fb = hal_display_get_framebuffer();
    ed->damage.rect = (rect_t){0, 0, fb->width, fb->height};
    ed->damage.kind = FLUSH_FULL;
}

// turn the rows a layout patch touched into damaged pixels
static void editor_damage_from_patch(editor_t *ed, layout_patch_t patch) {
    if (patch.recomputed || patch.first_line >= ed->layout.count) {
        editor_mark_full(ed);
        return;
    }
    const hal_framebuffer_t *fb = hal_display_get_framebuffer();
    int top = ed->layout.lines[patch.first_line].y - ed->font->ascent;
    int bottom;
    if (patch.to_end) {
        bottom = fb->height; // rows below shifted, clear down to old content
    } else {
        const layout_line_t *last = &ed->layout.lines[patch.last_line];
        bottom = last->y - ed->font->ascent + ed->font->line_height;
    }
    ed->damage.rect = rect_union(ed->damage.rect, (rect_t){0, top, fb->width, bottom - top});
}

// apply a content mutation to the standing layout so render never recomputes
static void editor_apply(editor_t *ed, edit_t edit) {
    layout_patch_t patch;
    if (!layout_apply_edit(&ed->layout, edit, ed->buf, ed->line_scratch, ed->line_scratch_capacity,
                           &patch)) {
        editor_mark_full(ed); // patch failed; layout may be reset, repaint all
        return;
    }
    editor_damage_from_patch(ed, patch);
}

editor_t *editor_create(const font_t *font, size_t document_capacity) {
    if (font == NULL || document_capacity == 0) {
        return NULL;
    }
    if (font->glyph_count == 0) {
        return NULL; // no glyphs, so no cursor width
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

    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    int wrap_width = fb->width - MARGIN_X;
    int x_advance = font->glyphs[0].x_advance;
    // a line holds at most wrap_width/x_advance glyphs, each up to 4 UTF-8 bytes
    // anything larger only exists by wrapping, which falls back to recompute
    size_t adv = x_advance > 0 ? (size_t)x_advance : 1;
    size_t line_cap = ((size_t)(wrap_width > 0 ? wrap_width : 0) / adv + 2) * 4;
    ed->line_scratch = hal_mem_alloc(line_cap, HAL_MEM_LARGE);
    if (ed->line_scratch == NULL) {
        buffer_destroy(ed->buf);
        hal_mem_free(ed);
        return NULL;
    }

    ed->line_scratch_capacity = line_cap;
    ed->layout = (layout_t){0};
    ed->font = font;
    ed->cursor_width = x_advance;
    ed->preferred_col = SIZE_MAX; // unset
    ed->last_cursor_rect = (rect_t){0};
    // the first frame paints the whole screen
    ed->damage = (damage_t){.rect = {0, 0, fb->width, fb->height}, .kind = FLUSH_FULL};

    // build the initial empty layout so apply_edit always has one to patch
    if (!layout_compute(&ed->layout, "", 0, font, wrap_width, MARGIN_X, MARGIN_TOP)) {
        hal_mem_free(ed->line_scratch);
        buffer_destroy(ed->buf);
        hal_mem_free(ed);
        return NULL;
    }
    return ed;
}

void editor_destroy(editor_t *ed) {
    if (ed == NULL) {
        return;
    }
    hal_mem_free(ed->line_scratch);
    buffer_destroy(ed->buf);
    layout_destroy(&ed->layout);
    hal_mem_free(ed);
}

bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len) {
    size_t pos = buffer_cursor_pos(ed->buf);
    bool changed = buffer_insert_bytes(ed->buf, bytes, len);
    if (changed) {
        editor_apply(ed, (edit_t){.type = EDIT_INSERT, .pos = pos, .len = len});
        ed->preferred_col = SIZE_MAX;
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
    editor_apply(ed, (edit_t){.type = EDIT_DELETE, .pos = prev_boundary, .len = deleted});
    ed->preferred_col = SIZE_MAX;
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
    editor_apply(ed, (edit_t){.type = EDIT_DELETE, .pos = cursor_pos, .len = deleted});
    ed->preferred_col = SIZE_MAX;
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
        ed->damage.rect = rect_union(ed->damage.rect, ed->last_cursor_rect);
        return true;
    }

    // up / down: the standing layout is already current
    size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_pos);

    if (direction == EDITOR_CURSOR_UP) {
        if (line_idx == 0)
            return false;
    } else {
        if (line_idx + 1 >= ed->layout.count)
            return false;
    }

    const layout_line_t *cur = &ed->layout.lines[line_idx];
    buffer_region_t cur_region = buffer_region(ed->buf, cur->byte_start, cursor_pos,
                                               ed->line_scratch, ed->line_scratch_capacity);
    size_t cur_col = utf8_count_codepoints(cur_region.ptr, cur_region.len);
    if (ed->preferred_col == SIZE_MAX)
        ed->preferred_col = cur_col;

    size_t target_idx = (direction == EDITOR_CURSOR_UP) ? line_idx - 1 : line_idx + 1;
    const layout_line_t *target_line = &ed->layout.lines[target_idx];
    buffer_region_t target_region =
        buffer_region(ed->buf, target_line->byte_start, target_line->byte_end, ed->line_scratch,
                      ed->line_scratch_capacity);
    size_t target_cps = utf8_count_codepoints(target_region.ptr, target_region.len);
    size_t target_col = ed->preferred_col < target_cps ? ed->preferred_col : target_cps;

    size_t offset_in_line =
        utf8_advance_codepoints(target_region.ptr, target_region.len, target_col);
    buffer_set_cursor(ed->buf, target_line->byte_start + offset_in_line);
    ed->damage.rect = rect_union(ed->damage.rect, ed->last_cursor_rect);
    return true;
}

damage_t editor_render(editor_t *ed) {
    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    rect_t clip = ed->damage.rect;

    // erase and redraw only the damaged region
    render_fill_rect(fb, clip.x, clip.y, clip.w, clip.h, BACKGROUND);

    // lines run top to bottom, so skip past those above the damage and stop
    // once one falls below it
    for (size_t i = 0; i < ed->layout.count; i++) {
        const layout_line_t *l = &ed->layout.lines[i];
        int top = l->y - ed->font->ascent;
        if (top >= clip.y + clip.h)
            break;
        if (top + ed->font->line_height <= clip.y)
            continue;
        buffer_region_t r = buffer_region(ed->buf, l->byte_start, l->byte_end, ed->line_scratch,
                                          ed->line_scratch_capacity);
        render_draw_string(fb, ed->font, MARGIN_X, l->y, r.ptr, r.len, clip);
    }

    size_t cursor_byte = buffer_cursor_pos(ed->buf);
    size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_byte);
    const layout_line_t *line = &ed->layout.lines[line_idx];
    buffer_region_t cursor_region = buffer_region(ed->buf, line->byte_start, cursor_byte,
                                                  ed->line_scratch, ed->line_scratch_capacity);
    size_t cps_before_cursor = utf8_count_codepoints(cursor_region.ptr, cursor_region.len);
    int cursor_x = MARGIN_X + (int)cps_before_cursor * ed->cursor_width;
    int cursor_y = line->y;

    render_fill_rect(fb, cursor_x, cursor_y + CURSOR_Y_OFFSET, ed->cursor_width, CURSOR_HEIGHT,
                     FOREGROUND);

    // include the new cursor cell in the flush and remember it for the next move
    rect_t cursor_rect = editor_cursor_rect(ed, cursor_x, cursor_y);
    ed->damage.rect = rect_union(ed->damage.rect, cursor_rect);
    ed->last_cursor_rect = cursor_rect;

    damage_t out = ed->damage;
    ed->damage = (damage_t){0}; // kind resets to FLUSH_PARTIAL
    return out;
}

size_t editor_get_size(const editor_t *ed) {
    return buffer_size(ed->buf);
}

bool editor_is_dirty(const editor_t *ed) {
    return !rect_is_empty(ed->damage.rect);
}

const buffer_t *editor_test_get_buffer(const editor_t *ed) {
    return ed->buf;
}
