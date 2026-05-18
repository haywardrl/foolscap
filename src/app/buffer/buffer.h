#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct buffer buffer_t;

// Lifecycle
buffer_t *buffer_create(size_t capacity);
void buffer_destroy(buffer_t *buffer);

// Modification — operate at the current cursor position.
// All return true on success, false on failure (e.g. capacity exceeded,
// cursor at boundary). On false return, buffer state is unchanged.

// Atomic multi-byte insert. Either all `len` bytes are inserted, or none.
// Useful for UTF-8 sequences (1-4 bytes per codepoint) — caller guarantees
// `bytes` contains complete codepoint sequences.
bool buffer_insert_bytes(buffer_t *buffer, const char *bytes, size_t len);

// Delete up to `n` bytes immediately before the cursor. Returns the number
// of bytes actually deleted (may be less than `n` if cursor was near start).
// O(1) — does not loop.
size_t buffer_delete_before_cursor(buffer_t *buffer, size_t n);

// Delete one byte at the cursor position. Returns true if a byte was deleted,
// false if the cursor was at end-of-buffer.
// Note: byte-oriented. Caller is responsible for UTF-8 awareness if deleting
// multi-byte sequences.
bool buffer_delete_forward(buffer_t *buffer);

// Cursor movement — both clamp to [0, buffer_size()].
// buffer_move_cursor returns true if the cursor's position actually changed,
// false if the delta resulted in no movement.
bool buffer_move_cursor(buffer_t *buffer, int32_t delta);
bool buffer_set_cursor(buffer_t *buffer, size_t pos);

// UTF-8 boundary helpers. Walk the buffer's bytes (gap-aware) and return the
// byte position of the codepoint boundary at-or-before / at-or-after `pos`.
// `pos` is clamped to [0, buffer_size()] before walking. Malformed UTF-8 is
// handled defensively: returns the nearest plausible boundary rather than
// looping. Boundaries returned are always in [0, buffer_size()].
size_t buffer_prev_codepoint_boundary(const buffer_t *buffer, size_t pos);
size_t buffer_next_codepoint_boundary(const buffer_t *buffer, size_t pos);

// Flatten the buffer's logical contents into `dst`. Returns the number of
// bytes written. If `dst_capacity < buffer_size()`, writes nothing and
// returns 0.
size_t buffer_copy_contiguous(const buffer_t *buffer, char *dst, size_t dst_capacity);

// Read-only accessors
size_t buffer_cursor_pos(const buffer_t *buffer);
size_t buffer_size(const buffer_t *buffer);

// Returns the byte at logical position `pos`. Returns '\0' if `pos` is out
// of range. Use buffer_size() to determine valid range; '\0' is also a
// legitimate buffer content, so don't use it as an in-band error signal.
char buffer_char_at(const buffer_t *buffer, size_t pos);
