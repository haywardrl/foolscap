#include "buffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct buffer {
    char *data;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
};

// --- Lifecycle ---

buffer_t *buffer_create(size_t capacity) {
    buffer_t *buffer = malloc(sizeof(*buffer));
    if (buffer == NULL) {
        return NULL;
    }
    if ((buffer->data = malloc(capacity)) == NULL) {
        free(buffer);
        return NULL;
    }
    buffer->capacity = capacity;
    buffer->gap_start = 0;
    buffer->gap_end = capacity;
    return buffer;
}

void buffer_destroy(buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    free(buffer);
}

// --- Modification ---

// Atomic: checks capacity first, then memcpys. On failure, no state change.
bool buffer_insert_bytes(buffer_t *buffer, const char *bytes, size_t len) {
    if (len == 0) {
        return true; // vacuously successful; no work to do
    }
    size_t gap_size = buffer->gap_end - buffer->gap_start;
    if (len > gap_size) {
        return false;
    }
    memcpy(buffer->data + buffer->gap_start, bytes, len);
    buffer->gap_start += len;
    return true;
}

size_t buffer_delete_before_cursor(buffer_t *buffer, size_t n) {
    size_t deletable = buffer->gap_start;
    if (n > deletable) {
        n = deletable;
    }
    buffer->gap_start -= n;
    return n;
}

bool buffer_delete_forward(buffer_t *buffer) {
    if (buffer->gap_end == buffer->capacity) {
        return false;
    }
    buffer->gap_end += 1;
    return true;
}

// --- Cursor movement ---

bool buffer_move_cursor(buffer_t *buffer, int32_t delta) {
    size_t current = buffer_cursor_pos(buffer);
    size_t new_pos;
    if (delta < 0) {
        size_t magnitude = (size_t)(-delta);
        if (magnitude > current) {
            new_pos = 0;
        } else {
            new_pos = current - magnitude;
        }
    } else {
        new_pos = current + (size_t)delta;
    }
    buffer_set_cursor(buffer, new_pos);
    return buffer_cursor_pos(buffer) != current;
}

bool buffer_set_cursor(buffer_t *buffer, size_t pos) {
    size_t before = buffer->gap_start;
    if (pos > buffer_size(buffer)) {
        pos = buffer_size(buffer);
    }
    // Move gap leftward
    while (buffer->gap_start > pos) {
        buffer->gap_start -= 1;
        buffer->gap_end -= 1;
        buffer->data[buffer->gap_end] = buffer->data[buffer->gap_start];
    }
    // Move gap rightward
    while (buffer->gap_start < pos) {
        buffer->data[buffer->gap_start] = buffer->data[buffer->gap_end];
        buffer->gap_start += 1;
        buffer->gap_end += 1;
    }
    return buffer->gap_start != before;
}

// --- UTF-8 boundary helpers ---

// A UTF-8 continuation byte matches 10xxxxxx — i.e. (b & 0xC0) == 0x80.
// A boundary is any byte that is NOT a continuation byte.
static bool is_continuation_byte(unsigned char b) {
    return (b & 0xC0) == 0x80;
}

size_t buffer_prev_codepoint_boundary(const buffer_t *buffer, size_t pos) {
    size_t size = buffer_size(buffer);
    if (pos > size) {
        pos = size;
    }
    if (pos == 0) {
        return 0;
    }
    // Walk backward at most 4 bytes (max UTF-8 sequence length).
    // If we can't find a boundary in 4 bytes, the data is malformed —
    // return pos - 1 as a defensive fallback.
    size_t i = pos;
    for (int steps = 0; steps < 4 && i > 0; steps++) {
        i -= 1;
        unsigned char b = (unsigned char)buffer_char_at(buffer, i);
        if (!is_continuation_byte(b)) {
            return i;
        }
    }
    return pos - 1;
}

size_t buffer_next_codepoint_boundary(const buffer_t *buffer, size_t pos) {
    size_t size = buffer_size(buffer);
    if (pos >= size) {
        return size;
    }
    // Step past the current byte, then walk forward up to 4 bytes until we
    // find a non-continuation byte (start of next codepoint) or hit the end.
    size_t i = pos + 1;
    for (int steps = 0; steps < 4 && i < size; steps++) {
        unsigned char b = (unsigned char)buffer_char_at(buffer, i);
        if (!is_continuation_byte(b)) {
            return i;
        }
        i += 1;
    }
    return i; // either reached end or walked 4 bytes (malformed)
}

// --- Flatten ---

size_t buffer_copy_contiguous(const buffer_t *buffer, char *dst, size_t dst_capacity) {
    size_t needed = buffer_size(buffer);
    if (dst_capacity < needed) {
        return 0;
    }
    size_t before_gap = buffer->gap_start;
    size_t after_gap = buffer->capacity - buffer->gap_end;
    if (before_gap > 0) {
        memcpy(dst, buffer->data, before_gap);
    }
    if (after_gap > 0) {
        memcpy(dst + before_gap, buffer->data + buffer->gap_end, after_gap);
    }
    return needed;
}

// --- Accessors ---

size_t buffer_cursor_pos(const buffer_t *buffer) {
    return buffer->gap_start;
}

size_t buffer_size(const buffer_t *buffer) {
    return buffer->gap_start + (buffer->capacity - buffer->gap_end);
}

char buffer_char_at(const buffer_t *buffer, size_t pos) {
    if (pos >= buffer_size(buffer)) {
        return '\0';
    }
    if (pos < buffer->gap_start) {
        return buffer->data[pos];
    }
    size_t gap_size = buffer->gap_end - buffer->gap_start;
    return buffer->data[pos + gap_size];
}
