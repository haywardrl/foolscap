#include "buffer.h"

#include "hal/hal_mem.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct buffer {
    char *data;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
};

buffer_t *buffer_create(size_t capacity) {
    buffer_t *buffer = hal_mem_alloc(sizeof(*buffer), HAL_MEM_DEFAULT);
    if (buffer == NULL) {
        return NULL;
    }
    buffer->data = hal_mem_alloc(capacity, HAL_MEM_LARGE);
    if (buffer->data == NULL) {
        hal_mem_free(buffer);
        return NULL;
    }
    buffer->capacity = capacity;
    buffer->gap_start = 0;
    buffer->gap_end = capacity;
    return buffer;
}

void buffer_destroy(buffer_t *buffer) {
    if (buffer == NULL)
        return;
    hal_mem_free(buffer->data);
    hal_mem_free(buffer);
}

// no partial insert; either all len bytes fit or none
bool buffer_insert_bytes(buffer_t *buffer, const char *bytes, size_t len) {
    if (len == 0) {
        return true;
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

size_t buffer_delete_after_cursor(buffer_t *buffer, size_t n) {
    size_t deletable = buffer->capacity - buffer->gap_end;
    if (n > deletable) {
        n = deletable;
    }
    buffer->gap_end += n;
    return n;
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

// continuation byte: 10xxxxxx
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
    // walk back at most 4 bytes (max UTF-8 seq)
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
    size_t i = pos + 1;
    for (int steps = 0; steps < 4 && i < size; steps++) {
        unsigned char b = (unsigned char)buffer_char_at(buffer, i);
        if (!is_continuation_byte(b)) {
            return i;
        }
        i += 1;
    }
    return i;
}

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

buffer_spans_t buffer_spans(const buffer_t *buffer) {
    return (buffer_spans_t){
        .first = buffer->data,
        .first_len = buffer->gap_start,
        .second = buffer->data + buffer->gap_end,
        .second_len = buffer->capacity - buffer->gap_end,
    };
}

buffer_region_t buffer_region(const buffer_t *buffer, size_t start, size_t end, char *scratch,
                              size_t scratch_cap) {
    size_t first_len = buffer->gap_start;
    if (end <= first_len) {
        // wholly before the gap
        return (buffer_region_t){.ptr = buffer->data + start, .len = end - start};
    }
    if (start >= first_len) {
        // wholly after the gap
        return (buffer_region_t){.ptr = buffer->data + buffer->gap_end + (start - first_len),
                                 .len = end - start};
    }
    // straddles the gap
    size_t len = end - start;
    if (scratch_cap < len) {
        return (buffer_region_t){.ptr = NULL, .len = 0};
    }
    size_t before = first_len - start;
    memcpy(scratch, buffer->data + start, before);
    memcpy(scratch + before, buffer->data + buffer->gap_end, end - first_len);
    return (buffer_region_t){.ptr = scratch, .len = len};
}

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
