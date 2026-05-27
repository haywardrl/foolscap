#pragma once

#include "app/render/font.h"
#include "app/render/rect.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct editor editor_t;

// how a damaged region should reach the panel: a fast partial refresh, or a
// slow anti-ghost full refresh (relayout, modal open/close).
typedef enum {
    FLUSH_PARTIAL,
    FLUSH_FULL,
} flush_kind_t;

// pixels changed this frame plus how to push them, an empty rect means nothing
// to flush.
typedef struct {
    rect_t rect;
    flush_kind_t kind;
} damage_t;

typedef enum {
    EDITOR_CURSOR_LEFT,
    EDITOR_CURSOR_RIGHT,
    EDITOR_CURSOR_UP,
    EDITOR_CURSOR_DOWN,
} editor_cursor_direction_t;

// returns NULL on alloc failure, null font, or zero capacity
editor_t *editor_create(const font_t *font, size_t document_capacity);

void editor_destroy(editor_t *ed);

// mutations mark dirty on success, return false if nothing changed

// caller must pass complete codepoint sequences
bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len);

// Delete the codepoint immediately before the cursor
bool editor_backspace(editor_t *ed);

// Delete the codepoint at the cursor
bool editor_delete_forward(editor_t *ed);

// Move the cursor by one codepoint in the given direction
bool editor_move_cursor(editor_t *ed, editor_cursor_direction_t direction);

// repaint the accumulated damage, then return it (and reset) so the caller
// flushes only the changed region, lines outside the region are left untouched.
damage_t editor_render(editor_t *ed);

// Status bar fields
size_t editor_get_size(const editor_t *ed);

// coalesce mutations, render once if dirty
bool editor_is_dirty(const editor_t *ed);
