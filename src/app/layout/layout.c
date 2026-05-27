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
    out->font = font;
    out->wrap_width = wrap_width;
    out->margin_x = margin_x;
    out->margin_top = margin_top;
    out->x_advance = font->glyphs[0].x_advance;

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
            // byte_end excludes the \n itself, line_start skips past it
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
        layout_line_t *grown = hal_mem_alloc(new_capacity * sizeof(layout_line_t), HAL_MEM_LARGE);
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

// rightmost line whose byte_start <= byte
size_t layout_find_line_for_byte(const layout_t *out, size_t byte) {
    size_t lo = 0, hi = out->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (out->lines[mid].byte_start <= byte) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo == 0 ? 0 : lo - 1; // lo = first line past byte so step back one
}

// full relayout, reusing the stored wrap context. cold path only, allocates a copy of the
// buffer rather than holding a document-sized scratch
static bool layout_recompute(layout_t *layout, const buffer_t *buf) {
    const font_t *font = layout->font;
    int wrap_width = layout->wrap_width;
    int margin_x = layout->margin_x;
    int margin_top = layout->margin_top;
    size_t needed = buffer_size(buf);
    char *tmp = NULL;
    if (needed > 0) {
        tmp = hal_mem_alloc(needed, HAL_MEM_LARGE);
        if (tmp == NULL) {
            return false;
        }
        buffer_copy_contiguous(buf, tmp, needed);
    }
    layout_destroy(layout);
    bool ok = layout_compute(layout, needed > 0 ? tmp : "", needed, font, wrap_width, margin_x,
                             margin_top);
    if (tmp != NULL) {
        hal_mem_free(tmp);
    }
    return ok;
}

// open a gap at idx so lines[idx] becomes a free slot and count grows by one
static bool layout_insert_line_at(layout_t *layout, size_t idx) {
    if (layout->count + 1 > layout->capacity) {
        size_t new_cap = layout->capacity ? layout->capacity * 2 : 16;
        layout_line_t *grown = hal_mem_alloc(new_cap * sizeof(layout_line_t), HAL_MEM_LARGE);
        if (!grown)
            return false;
        if (layout->lines) {
            memcpy(grown, layout->lines, layout->count * sizeof(layout_line_t));
            hal_mem_free(layout->lines);
        }
        layout->lines = grown;
        layout->capacity = new_cap;
    }
    if (idx < layout->count) {
        memmove(&layout->lines[idx + 1], &layout->lines[idx],
                (layout->count - idx) * sizeof(layout_line_t));
    }
    layout->count++;
    return true;
}

// remove the entry at idx, count shrinks by one, idx must be < count
static void layout_remove_line_at(layout_t *layout, size_t idx) {
    if (idx + 1 < layout->count) {
        memmove(&layout->lines[idx], &layout->lines[idx + 1],
                (layout->count - idx - 1) * sizeof(layout_line_t));
    }
    layout->count--;
}

bool layout_apply_edit(layout_t *layout, edit_t edit, const buffer_t *buf, char *scratch,
                       size_t scratch_cap, layout_patch_t *out) {
    *out = (layout_patch_t){0};
    size_t li = layout_find_line_for_byte(layout, edit.pos);
    layout_line_t *line = &layout->lines[li];

    if (edit.type == EDIT_INSERT) {
        // a newline splits the line
        buffer_region_t ins =
            buffer_region(buf, edit.pos, edit.pos + edit.len, scratch, scratch_cap);
        if (memchr(ins.ptr, '\n', ins.len) != NULL) {
            size_t orig_byte_end = line->byte_end;
            bool orig_ends_with_newline = line->ends_with_newline;
            int orig_y = line->y;

            if (!layout_insert_line_at(layout, li + 1)) {
                out->recomputed = true;
                return layout_recompute(layout, buf);
            }
            // layout->lines may have moved, re-index instead of using original line
            layout->lines[li].byte_end = edit.pos;
            layout->lines[li].ends_with_newline = true;

            layout->lines[li + 1].byte_start = edit.pos + 1;
            layout->lines[li + 1].byte_end = orig_byte_end + 1;
            layout->lines[li + 1].y = orig_y + layout->font->line_height;
            layout->lines[li + 1].ends_with_newline = orig_ends_with_newline;

            for (size_t i = li + 2; i < layout->count; i++) {
                layout->lines[i].byte_start += 1;
                layout->lines[i].byte_end += 1;
                layout->lines[i].y += layout->font->line_height;
            }
            out->first_line = li;
            out->to_end = true;
            return true;
        }
        // growth pushes the line past the margin, so it wraps
        size_t new_end = line->byte_end + edit.len;
        buffer_region_t lr = buffer_region(buf, line->byte_start, new_end, scratch, scratch_cap);
        if (lr.ptr == NULL) {
            out->recomputed = true;
            return layout_recompute(layout, buf); // line too big for scratch -> it wraps anyway
        }
        size_t cps = utf8_count_codepoints(lr.ptr, lr.len);
        if (layout->margin_x + (int)cps * layout->x_advance > layout->wrap_width) {
            out->recomputed = true;
            return layout_recompute(layout, buf);
        }
    } else { // EDIT_DELETE
        // merge: deleting the single newline terminating this line joins it
        // with the next line, provided the combined line still fits
        if (edit.len == 1 && edit.pos == line->byte_end && line->ends_with_newline) {
            size_t merged_end = layout->lines[li + 1].byte_end - 1;
            bool merged_ends_with_newline = layout->lines[li + 1].ends_with_newline;

            // if the combined line overflows it would re-wrap
            buffer_region_t mr =
                buffer_region(buf, line->byte_start, merged_end, scratch, scratch_cap);
            if (mr.ptr == NULL) {
                out->recomputed = true;
                return layout_recompute(layout, buf); // merged line too big -> it wraps anyway
            }
            size_t cps = utf8_count_codepoints(mr.ptr, mr.len);
            if (layout->margin_x + (int)cps * layout->x_advance > layout->wrap_width) {
                out->recomputed = true;
                return layout_recompute(layout, buf);
            }

            layout->lines[li].byte_end = merged_end;
            layout->lines[li].ends_with_newline = merged_ends_with_newline;
            layout_remove_line_at(layout, li + 1);

            for (size_t i = li + 1; i < layout->count; i++) {
                layout->lines[i].byte_start -= 1;
                layout->lines[i].byte_end -= 1;
                layout->lines[i].y -= layout->font->line_height;
            }
            out->first_line = li;
            out->to_end = true;
            return true;
        }
        // deletion reaches past the line's content, so it removed a newline or
        // spans lines that must merge
        if (edit.pos + edit.len > line->byte_end) {
            out->recomputed = true;
            return layout_recompute(layout, buf);
        }
        // a soft-wrapped line can de-wrap when it shrinks, pulling words up from
        // the next line
        bool is_last = (li + 1 == layout->count);
        if (!line->ends_with_newline && !is_last) {
            out->recomputed = true;
            return layout_recompute(layout, buf);
        }
    }

    // offsets only: the edited line grows/shrinks and every line below shifts by
    // the same amount. no break moved, so y and ends_with_newline hold, only
    // the edited row repaints
    ptrdiff_t delta = (edit.type == EDIT_INSERT) ? (ptrdiff_t)edit.len : -(ptrdiff_t)edit.len;
    line->byte_end += delta;
    for (size_t i = li + 1; i < layout->count; i++) {
        layout->lines[i].byte_start += delta;
        layout->lines[i].byte_end += delta;
    }
    out->first_line = li;
    out->last_line = li;
    return true;
}
