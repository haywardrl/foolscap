#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct buffer buffer_t;

// Lifecycle
buffer_t *buffer_create(size_t capacity);
void buffer_destroy(buffer_t *buffer);

// operate at cursor; return true on success, false on failure (state unchanged)

// atomic: inserts all len bytes or none
bool buffer_insert_bytes(buffer_t *buffer, const char *bytes, size_t len);

// returns bytes deleted (may be less than n)
size_t buffer_delete_before_cursor(buffer_t *buffer, size_t n);

// returns bytes deleted (may be less than n)
size_t buffer_delete_after_cursor(buffer_t *buffer, size_t n);

// clamps to [0, buffer_size()]
bool buffer_set_cursor(buffer_t *buffer, size_t pos);

// gap-aware; clamps pos; handles malformed UTF-8 defensively
size_t buffer_prev_codepoint_boundary(const buffer_t *buffer, size_t pos);
size_t buffer_next_codepoint_boundary(const buffer_t *buffer, size_t pos);

// writes nothing and returns 0 if dst_capacity < buffer_size()
size_t buffer_copy_contiguous(const buffer_t *buffer, char *dst, size_t dst_capacity);

// Read-only accessors
size_t buffer_cursor_pos(const buffer_t *buffer);
size_t buffer_size(const buffer_t *buffer);

// returns '\0' for out-of-range (not a reliable EOF signal)
char buffer_char_at(const buffer_t *buffer, size_t pos);
