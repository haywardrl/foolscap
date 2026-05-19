#pragma once

#include "app/render/font.h"
#include "hal/hal_display.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct editor editor_t;

typedef enum {
    EDITOR_CURSOR_LEFT,
    EDITOR_CURSOR_RIGHT,
} editor_cursor_direction_t;

// returns NULL on alloc failure, null font, or zero capacity
editor_t *editor_create(const font_t *font, size_t document_capacity);

void editor_destroy(editor_t *ed);

// mutations mark dirty on success; return false if nothing changed

// caller must pass complete codepoint sequences
bool editor_insert_utf8(editor_t *ed, const char *bytes, size_t len);

// Delete the codepoint immediately before the cursor.
bool editor_backspace(editor_t *ed);

// Delete the codepoint at the cursor.
bool editor_delete_forward(editor_t *ed);

// Move the cursor by one codepoint in the given direction.
bool editor_move_cursor(editor_t *ed, editor_cursor_direction_t direction);

// unconditional; clears dirty flag
void editor_render(editor_t *ed);

// Status bar fields
size_t editor_get_size(const editor_t *ed);

// coalesce mutations; render once if dirty
bool editor_is_dirty(const editor_t *ed);
