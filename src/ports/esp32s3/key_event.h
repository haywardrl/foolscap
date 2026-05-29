#pragma once

#include <stdint.h>

// A decoded key, produced by the input source and consumed by the editor loop.
// Portable (no ESP/FreeRTOS deps) so the HID decoder can be unit-tested on host.
typedef enum {
    KEY_INSERT, // bytes/len carry a UTF-8 codepoint
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_WORD_LEFT,
    KEY_WORD_RIGHT,
    KEY_LINE_START,
    KEY_LINE_END,
    KEY_UNDO,
    KEY_REDO,
} key_kind_t;

typedef struct {
    key_kind_t kind;
    char bytes[4];
    uint8_t len;
} key_event_t;
