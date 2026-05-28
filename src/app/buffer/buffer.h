#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct buffer buffer_t;

// Lifecycle
buffer_t *buffer_create(size_t capacity);
void buffer_destroy(buffer_t *buffer);

// atomic: inserts all len bytes or none
bool buffer_insert_bytes(buffer_t *buffer, const char *bytes, size_t len);

// returns bytes deleted (may be less than n)
size_t buffer_delete_before_cursor(buffer_t *buffer, size_t n);

// returns bytes deleted (may be less than n)
size_t buffer_delete_after_cursor(buffer_t *buffer, size_t n);

// clamps to [0, buffer_size()]
bool buffer_set_cursor(buffer_t *buffer, size_t pos);

// gap-aware, clamps pos
size_t buffer_prev_codepoint_boundary(const buffer_t *buffer, size_t pos);
size_t buffer_next_codepoint_boundary(const buffer_t *buffer, size_t pos);
// readline version, jumps to end or start of word, not like vim-w
size_t buffer_next_word_boundary(const buffer_t *buffer, size_t pos);
size_t buffer_prev_word_boundary(const buffer_t *buffer, size_t pos);

// writes nothing and returns 0 if dst_capacity < buffer_size()
size_t buffer_copy_contiguous(const buffer_t *buffer, char *dst, size_t dst_capacity);

// the two contiguous runs either side of the gap, invalidated by any mutation (set_cursor shuffles
// bytes), so use transiently, never across an edit
typedef struct {
    const char *first;
    size_t first_len;
    const char *second;
    size_t second_len;
} buffer_spans_t;

buffer_spans_t buffer_spans(const buffer_t *buffer);

// logical bytes [start, end) as one run
typedef struct {
    const char *ptr;
    size_t len;
} buffer_region_t;

buffer_region_t buffer_region(const buffer_t *buffer, size_t start, size_t end, char *scratch,
                              size_t scratch_cap);

// Read-only accessors
size_t buffer_cursor_pos(const buffer_t *buffer);
size_t buffer_size(const buffer_t *buffer);

// returns '\0' for out-of-range
char buffer_char_at(const buffer_t *buffer, size_t pos);
