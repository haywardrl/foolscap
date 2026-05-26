#pragma once

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

// Safe to call on a zero-initialised or post-compute layout_t.
void layout_destroy(layout_t *layout);

// On failure, out is left in a destroyed (zeroed) state.
bool layout_compute(layout_t *layout, const char *text, size_t len, const font_t *font,
                    int wrap_width, int margin_x, int margin_top);

size_t layout_find_line_for_byte(const layout_t *l, size_t byte);

bool layout_apply_edit(layout_t *layout, edit_t edit, const char *text, size_t len);
