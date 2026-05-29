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
#include <string.h>

// undo/redo depth. small enough to keep the inline rings light, large enough
// to hit ring wrap and eviction in tests.
#define UNDO_CAP 256

typedef enum { CMD_INSERT, CMD_DELETE } command_kind_t;
typedef struct {
    command_kind_t kind;
    size_t pos;
    size_t len;
    char *bytes; // owned by whichever stack holds the command
} command_t;

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

    // undo is a ring (drops oldest when full). redo is a stack that can never
    // outgrow it, so it shares the cap. every live entry owns its bytes.
    command_t undo[UNDO_CAP];
    size_t undo_head;  // next push slot
    size_t undo_count; // live entries, <= UNDO_CAP
    command_t redo[UNDO_CAP];
    size_t redo_count;   // top is redo[redo_count - 1]
    bool coalesce_break; // next edit starts a new undo group (cursor jump/undo/redo)
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
    if (patch.recomputed) {
        // recompute lost per-line damage info, so repaint the whole screen as
        // a partial (1-bit) flush. periodic full refresh handles ghosting.
        const hal_framebuffer_t *fb = hal_display_get_framebuffer();
        ed->damage.rect = rect_union(ed->damage.rect, (rect_t){0, 0, fb->width, fb->height});
        return;
    }
    if (patch.first_line >= ed->layout.count) {
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
static bool editor_apply(editor_t *ed, command_t cmd) {
    if (cmd.kind == CMD_INSERT) {
        buffer_set_cursor(ed->buf, cmd.pos);
        if (!buffer_insert_bytes(ed->buf, cmd.bytes, cmd.len))
            return false;
    } else { // CMD_DELETE
        // callers guarantee (pos, pos+len) is removable, so this can't fail
        buffer_set_cursor(ed->buf, cmd.pos + cmd.len);
        buffer_delete_before_cursor(ed->buf, cmd.len);
    }
    // patch layout and accumulate damage
    edit_t edit = {.type = (cmd.kind == CMD_INSERT) ? EDIT_INSERT : EDIT_DELETE,
                   .pos = cmd.pos,
                   .len = cmd.len};
    layout_patch_t patch;

    if (!layout_apply_edit(&ed->layout, edit, ed->buf, ed->line_scratch, ed->line_scratch_capacity,
                           &patch)) {
        editor_mark_full(ed); // patch failed; layout may be reset, repaint all
        return false;
    }
    editor_damage_from_patch(ed, patch);
    return true;
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
    ed->undo_head = 0;
    ed->undo_count = 0;
    ed->redo_count = 0;
    ed->coalesce_break = false;
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

// index of the i-th live undo entry, oldest (i=0) to newest (i=count-1)
static size_t undo_live_index(const editor_t *ed, size_t i) {
    return (ed->undo_head + UNDO_CAP - ed->undo_count + i) % UNDO_CAP;
}

// push a command onto the undo ring, taking ownership of cmd.bytes. when full,
// the slot at head holds the oldest command, so free its payload before reuse.
static void undo_push(editor_t *ed, command_t cmd) {
    if (ed->undo_count == UNDO_CAP) {
        hal_mem_free(ed->undo[ed->undo_head].bytes); // drop oldest
    } else {
        ed->undo_count++;
    }
    ed->undo[ed->undo_head] = cmd;
    ed->undo_head = (ed->undo_head + 1) % UNDO_CAP;
}

// free every redo payload and empty the stack. any new edit discards redo
// (undo is linear, not branching).
static void redo_clear(editor_t *ed) {
    for (size_t i = 0; i < ed->redo_count; i++) {
        hal_mem_free(ed->redo[i].bytes);
    }
    ed->redo_count = 0;
}

// pop the newest undo entry into *out, transferring ownership to the caller.
static bool undo_pop(editor_t *ed, command_t *out) {
    if (ed->undo_count == 0) {
        return false;
    }
    ed->undo_head = (ed->undo_head + UNDO_CAP - 1) % UNDO_CAP;
    *out = ed->undo[ed->undo_head];
    ed->undo_count--;
    return true;
}

// redo can never hold more than the undo history (each entry came from an undo
// pop), which is capped at UNDO_CAP, so the stack cannot overflow.
static void redo_push(editor_t *ed, command_t cmd) {
    ed->redo[ed->redo_count++] = cmd;
}

static bool redo_pop(editor_t *ed, command_t *out) {
    if (ed->redo_count == 0) {
        return false;
    }
    *out = ed->redo[--ed->redo_count];
    return true;
}

void editor_destroy(editor_t *ed) {
    if (ed == NULL) {
        return;
    }
    for (size_t i = 0; i < ed->undo_count; i++) {
        hal_mem_free(ed->undo[undo_live_index(ed, i)].bytes);
    }
    redo_clear(ed);
    hal_mem_free(ed->line_scratch);
    buffer_destroy(ed->buf);
    layout_destroy(&ed->layout);
    hal_mem_free(ed);
}

static bool bytes_have_newline(const char *b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (b[i] == '\n') {
            return true;
        }
    }
    return false;
}

// try to fold cmd into the newest undo entry, growing its payload in place.
// returns true (and frees cmd.bytes) on a merge. rules: same kind, contiguous
// range, no newline on either side. a newline is always its own undo step.
static bool try_coalesce(editor_t *ed, command_t cmd) {
    if (ed->undo_count == 0) {
        return false;
    }
    command_t *top = &ed->undo[(ed->undo_head + UNDO_CAP - 1) % UNDO_CAP];
    if (top->kind != cmd.kind) {
        return false;
    }
    if (bytes_have_newline(cmd.bytes, cmd.len) || bytes_have_newline(top->bytes, top->len)) {
        return false;
    }

    bool prepend = false; // delete extending leftward (backspace)
    if (cmd.kind == CMD_INSERT) {
        if (cmd.pos != top->pos + top->len) {
            return false; // typing must continue right after the run
        }
    } else { // CMD_DELETE
        if (cmd.pos == top->pos) {
            prepend = false; // delete-forward: new bytes follow top's
        } else if (cmd.pos + cmd.len == top->pos) {
            prepend = true; // backspace: new bytes precede top's
        } else {
            return false;
        }
    }

    size_t new_len = top->len + cmd.len;
    char *grown = hal_mem_alloc(new_len, HAL_MEM_DEFAULT);
    if (grown == NULL) {
        return false; // fall back to recording a separate command
    }
    if (prepend) {
        memcpy(grown, cmd.bytes, cmd.len);
        memcpy(grown + cmd.len, top->bytes, top->len);
        top->pos = cmd.pos; // the run's start moves left
    } else {
        memcpy(grown, top->bytes, top->len);
        memcpy(grown + top->len, cmd.bytes, cmd.len);
    }
    hal_mem_free(top->bytes);
    top->bytes = grown;
    top->len = new_len;
    hal_mem_free(cmd.bytes); // folded in; cmd's own copy is now redundant
    return true;
}

// apply cmd, then record it: fold into the current undo group when allowed,
// else start a new one. on failure, free the payload and report no change.
static bool editor_record(editor_t *ed, command_t cmd) {
    if (!editor_apply(ed, cmd)) {
        hal_mem_free(cmd.bytes);
        return false;
    }
    bool coalesced = !ed->coalesce_break && try_coalesce(ed, cmd);
    if (!coalesced) {
        undo_push(ed, cmd); // ring now owns the payload
    }
    ed->coalesce_break = false;
    redo_clear(ed);
    ed->preferred_col = SIZE_MAX;
    return true;
}

bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len) {
    // own a copy. the command outlives this call on the undo ring, so it can't
    // borrow the caller's transient bytes.
    char *owned = hal_mem_alloc(len, HAL_MEM_DEFAULT);
    if (owned == NULL) {
        return false;
    }
    memcpy(owned, bytes, len);
    size_t pos = buffer_cursor_pos(ed->buf);
    return editor_record(ed,
                         (command_t){.kind = CMD_INSERT, .pos = pos, .len = len, .bytes = owned});
}

bool editor_backspace(editor_t *ed) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    if (cursor_pos == 0) {
        return false;
    }
    size_t prev_boundary = buffer_prev_codepoint_boundary(ed->buf, cursor_pos);
    size_t len = cursor_pos - prev_boundary;
    char *owned = hal_mem_alloc(len, HAL_MEM_DEFAULT);
    if (owned == NULL) {
        return false;
    }
    // copy the bytes before the delete removes them, so undo can reinsert them
    buffer_region_t r = buffer_region(ed->buf, prev_boundary, cursor_pos, ed->line_scratch,
                                      ed->line_scratch_capacity);
    memcpy(owned, r.ptr, r.len);
    return editor_record(
        ed, (command_t){.kind = CMD_DELETE, .pos = prev_boundary, .len = len, .bytes = owned});
}

bool editor_delete_forward(editor_t *ed) {
    size_t cursor_pos = buffer_cursor_pos(ed->buf);
    size_t buffer_len = buffer_size(ed->buf);
    if (cursor_pos == buffer_len) {
        return false;
    }
    size_t next_boundary = buffer_next_codepoint_boundary(ed->buf, cursor_pos);
    size_t len = next_boundary - cursor_pos;
    char *owned = hal_mem_alloc(len, HAL_MEM_DEFAULT);
    if (owned == NULL) {
        return false;
    }
    buffer_region_t r = buffer_region(ed->buf, cursor_pos, next_boundary, ed->line_scratch,
                                      ed->line_scratch_capacity);
    memcpy(owned, r.ptr, r.len);
    return editor_record(
        ed, (command_t){.kind = CMD_DELETE, .pos = cursor_pos, .len = len, .bytes = owned});
}

bool editor_undo(editor_t *ed) {
    command_t cmd;
    if (!undo_pop(ed, &cmd)) {
        return false;
    }
    // the inverse only reads cmd.bytes, so no copy or ownership change here
    command_t inverse = cmd;
    inverse.kind = (cmd.kind == CMD_INSERT) ? CMD_DELETE : CMD_INSERT;
    // reinsert always fits (the bytes were just freed) and delete-by-position
    // always succeeds, so apply can't fail
    editor_apply(ed, inverse);
    redo_push(ed, cmd); // redo now owns the payload
    ed->coalesce_break = true;
    ed->preferred_col = SIZE_MAX;
    return true;
}

bool editor_redo(editor_t *ed) {
    command_t cmd;
    if (!redo_pop(ed, &cmd)) {
        return false;
    }
    editor_apply(ed, cmd); // replay the original edit forward
    undo_push(ed, cmd);    // undo owns the payload again
    ed->coalesce_break = true;
    ed->preferred_col = SIZE_MAX;
    return true;
}

bool editor_move_cursor(editor_t *ed, editor_cursor_direction_t direction) {
    ed->coalesce_break = true; // any cursor jump ends the current undo group
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

    if (direction == EDITOR_CURSOR_WORD_LEFT || direction == EDITOR_CURSOR_WORD_RIGHT) {
        size_t target;
        if (direction == EDITOR_CURSOR_WORD_LEFT) {
            if (cursor_pos == 0)
                return false;
            target = buffer_prev_word_boundary(ed->buf, cursor_pos);
        } else {
            if (cursor_pos == buffer_len)
                return false;
            target = buffer_next_word_boundary(ed->buf, cursor_pos);
        }
        buffer_set_cursor(ed->buf, target);
        ed->preferred_col = SIZE_MAX;
        ed->damage.rect = rect_union(ed->damage.rect, ed->last_cursor_rect);
        return true;
    }

    if (direction == EDITOR_CURSOR_LINE_START || direction == EDITOR_CURSOR_LINE_END) {
        size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_pos);
        const layout_line_t *line = &ed->layout.lines[line_idx];
        size_t target;
        if (direction == EDITOR_CURSOR_LINE_START) {
            target = line->byte_start;
        } else {
            target = line->byte_end;
            // soft-wrapped line: byte_end is the first byte of the next visual
            // row, so back off one codepoint to land at this row's last glyph.
            // skipped on the final layout line, where byte_end == buffer_size.
            bool is_soft_wrapped = !line->ends_with_newline && line_idx + 1 < ed->layout.count;
            if (is_soft_wrapped && target > line->byte_start) {
                target = buffer_prev_codepoint_boundary(ed->buf, target);
            }
        }
        if (target == cursor_pos)
            return false;
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

    size_t cursor_byte = buffer_cursor_pos(ed->buf);
    size_t line_idx = layout_find_line_for_byte(&ed->layout, cursor_byte);
    const layout_line_t *line = &ed->layout.lines[line_idx];
    buffer_region_t cursor_region = buffer_region(ed->buf, line->byte_start, cursor_byte,
                                                  ed->line_scratch, ed->line_scratch_capacity);
    size_t cps_before_cursor = utf8_count_codepoints(cursor_region.ptr, cursor_region.len);
    int cursor_x = MARGIN_X + (int)cps_before_cursor * ed->cursor_width;
    int cursor_y = line->y;
    rect_t cursor_rect = editor_cursor_rect(ed, cursor_x, cursor_y);

    // the cursor underline sits below the text row, so it can fall outside an
    // edited row's damage. fold old and new cursor cells in before erasing.
    ed->damage.rect = rect_union(ed->damage.rect, ed->last_cursor_rect);
    ed->damage.rect = rect_union(ed->damage.rect, cursor_rect);
    rect_t clip = ed->damage.rect;

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

    render_fill_rect(fb, cursor_x, cursor_y + CURSOR_Y_OFFSET, ed->cursor_width, CURSOR_HEIGHT,
                     FOREGROUND);

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

size_t editor_test_undo_count(const editor_t *ed) {
    return ed->undo_count;
}

size_t editor_test_redo_count(const editor_t *ed) {
    return ed->redo_count;
}
