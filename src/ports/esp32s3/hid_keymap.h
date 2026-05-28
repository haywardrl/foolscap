#pragma once

#include "key_event.h"

#include <stdbool.h>
#include <stdint.h>

// decode one USB HID boot-keyboard usage into a key event, US layout.
// returns false for keys we don't handle (caller drops them).
bool hid_decode(uint8_t mods, uint8_t keycode, bool caps_lock, key_event_t *out);
