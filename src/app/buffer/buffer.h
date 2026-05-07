#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct buffer buffer_t;

// Lifecycle
buffer_t *buffer_create(size_t capacity);
void buffer_destroy(buffer_t *buffer);

// Modification — operate at the current cursor position
// All return true if the buffer changed, false otherwise
bool buffer_insert_char(buffer_t *buffer, char c);
bool buffer_backspace(buffer_t *buffer);
bool buffer_delete_forward(buffer_t *buffer);

// Cursor movement
// Both clamp to valid range and return true if the cursor moved
bool buffer_move_cursor(buffer_t *buffer, int32_t delta);
bool buffer_set_cursor(buffer_t *buffer, size_t pos);

// Read-only accessors
size_t buffer_cursor_pos(const buffer_t *buffer);
size_t buffer_size(const buffer_t *buffer);
