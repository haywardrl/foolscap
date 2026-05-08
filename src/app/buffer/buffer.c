#include "buffer.h"

#include <stddef.h>
#include <stdlib.h>

struct buffer {
    char *data;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
};

// create buffer: must be a pointer as buffer_t is opaque
buffer_t *buffer_create(size_t capacity) {

    // allocate memory for a new buffer
    buffer_t *buffer = malloc(sizeof(*buffer));
    if (buffer == NULL) {
        return NULL;
    }

    // allocate memory to data the size of buffer capacity
    if ((buffer->data = malloc(capacity)) == NULL) {
        free(buffer);
        return NULL;
    }
    buffer->capacity = capacity;
    buffer->gap_start = 0;
    buffer->gap_end = capacity;

    return buffer;
}

// destroy buffer: free the data malloc first then the buffer_t
void buffer_destroy(buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    free(buffer);
}

// buffer_insert_char: inserts char at the cursor position
bool buffer_insert_char(buffer_t *buffer, char c) {
    if (buffer->gap_start == buffer->gap_end) {
        return false;
    }

    buffer->data[buffer->gap_start] = c;
    buffer->gap_start += 1;
    return true;
}

// buffer_backspace: remove char in buffer left of cursor
bool buffer_backspace(buffer_t *buffer) {
    if (buffer->gap_start == 0) {
        return false;
    }
    buffer->gap_start -= 1;
    buffer->gap_end -= 1;
    return true;
}

// buffer_delete_forward: remove char in buffer right of cursor
bool buffer_delete_forward(buffer_t *buffer) {
    size_t current = buffer_cursor_pos(buffer);
    if (current == buffer_size(buffer)) {
        return false;
    }
    buffer->gap_end += 1;
    return true;
}

// buffer_move_cursor: moves cursor left or right depending on delta
bool buffer_move_cursor(buffer_t *buffer, int32_t delta) {
    size_t current = buffer_cursor_pos(buffer);
    size_t new_pos;
    if (delta < 0) {
        // delta of -5 becomes 5
        size_t magnitude = (size_t)(-delta);
        if (magnitude > current) {
            new_pos = 0; // would cause an overflow, clamp to 0
        } else {
            new_pos = current - magnitude;
        }
    } else {
        new_pos = current + (size_t)delta;
    }
    buffer_set_cursor(buffer, new_pos);
    return true;
}

// buffer_set_cursor: position the cursor to a new location.
// Forgiving: clamps pos to [0, buffer_size()].
bool buffer_set_cursor(buffer_t *buffer, size_t pos) {
    // Clamp pos to a valid range
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

    return true;
}

// buffer_cursor_pos: returns where the cursor is located
size_t buffer_cursor_pos(const buffer_t *buffer) {
    return buffer->gap_start;
}

// buffer_size: returns the size of buffer exclusive of gap
size_t buffer_size(const buffer_t *buffer) {
    return buffer->gap_start + ((buffer->capacity - 1) - buffer->gap_end);
}
