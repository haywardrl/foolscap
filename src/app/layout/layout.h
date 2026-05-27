#pragma once

#include "app/buffer/buffer.h"
#include "app/render/font.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t byte_start; // first content byte (inclusive)
    size_t byte_end;   // one past last content byte (exclusive of any terminator)
    int y;
    bool ends_with_newline;
} layout_line_t;

typedef struct {
    layout_line_t *lines;
    size_t count;
    size_t capacity;
    const font_t *font;
    int wrap_width;
    int margin_x;
    int margin_top;
    int x_advance;
} layout_t;

typedef enum { EDIT_INSERT, EDIT_DELETE } edit_type_t;

typedef struct {
    edit_type_t type;
    size_t pos;
    size_t len;
} edit_t;

// Safe to call on a zero-initialised or post-compute layout_t
void layout_destroy(layout_t *layout);

// On failure, out is left in a destroyed (zeroed) state
bool layout_compute(layout_t *layout, const char *text, size_t len, const font_t *font,
                    int wrap_width, int margin_x, int margin_top);

size_t layout_find_line_for_byte(const layout_t *l, size_t byte);

// which lines a patch touched, so the caller can repaint just those rows
typedef struct {
    size_t first_line; // first line whose contents or position changed
    size_t last_line;  // inclusive, ignored when to_end is set
    bool to_end;       // change runs through the final line (rows below moved)
    bool recomputed;   // full relayout fallback so treat the whole text as changed
} layout_patch_t;

// patch layout to reflect edit applied to buf (which holds the post-edit
// text). per-line reads use scratch (one line's worth). Any edit that crosses a
// wrap boundary falls back to a full recompute. out reports the touched rows.
// returns false only on alloc failure
bool layout_apply_edit(layout_t *layout, edit_t edit, const buffer_t *buf, char *scratch,
                       size_t scratch_cap, layout_patch_t *out);
