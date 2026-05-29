#include "hid_keymap.h"

#include "key_event.h"

#define MOD_SHIFT 0x22 // left (0x02) | right (0x20) shift
#define MOD_CTRL 0x11  // left (0x01) | right (0x10) ctrl
#define MOD_ALT 0x44   // left (0x04) | right (0x40) alt
#define MOD_WORD (MOD_CTRL | MOD_ALT)

// HID usage -> ASCII, US layout. index is the usage id, 0 = not printable.
// covers 0x04..0x38 (letters, digits, space, common punctuation).
static const char unshifted[0x39] = {
    [0x04] = 'a', [0x05] = 'b',  [0x06] = 'c', [0x07] = 'd', [0x08] = 'e', [0x09] = 'f',
    [0x0A] = 'g', [0x0B] = 'h',  [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n',  [0x12] = 'o', [0x13] = 'p', [0x14] = 'q', [0x15] = 'r',
    [0x16] = 's', [0x17] = 't',  [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z',  [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6',  [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x2C] = ' ', [0x2D] = '-',  [0x2E] = '=', [0x2F] = '[', [0x30] = ']', [0x31] = '\\',
    [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',', [0x37] = '.', [0x38] = '/',
};

static const char shifted[0x39] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D', [0x08] = 'E', [0x09] = 'F',
    [0x0A] = 'G', [0x0B] = 'H', [0x0C] = 'I', [0x0D] = 'J', [0x0E] = 'K', [0x0F] = 'L',
    [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P', [0x14] = 'Q', [0x15] = 'R',
    [0x16] = 'S', [0x17] = 'T', [0x18] = 'U', [0x19] = 'V', [0x1A] = 'W', [0x1B] = 'X',
    [0x1C] = 'Y', [0x1D] = 'Z', [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$',
    [0x22] = '%', [0x23] = '^', [0x24] = '&', [0x25] = '*', [0x26] = '(', [0x27] = ')',
    [0x2C] = ' ', [0x2D] = '_', [0x2E] = '+', [0x2F] = '{', [0x30] = '}', [0x31] = '|',
    [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<', [0x37] = '>', [0x38] = '?',
};

bool hid_decode(uint8_t mods, uint8_t keycode, bool caps_lock, key_event_t *out) {
    switch (keycode) {
    case 0x28: // Enter
        out->kind = KEY_INSERT;
        out->bytes[0] = '\n';
        out->len = 1;
        return true;
    case 0x2A: // Backspace
        out->kind = KEY_BACKSPACE;
        out->len = 0;
        return true;
    case 0x4C: // Delete Forward
        out->kind = KEY_DELETE;
        out->len = 0;
        return true;
    case 0x4F: // Right
        out->kind = (mods & MOD_WORD) ? KEY_WORD_RIGHT : KEY_RIGHT;
        out->len = 0;
        return true;
    case 0x50: // Left
        out->kind = (mods & MOD_WORD) ? KEY_WORD_LEFT : KEY_LEFT;
        out->len = 0;
        return true;
    case 0x51: // Down
        out->kind = KEY_DOWN;
        out->len = 0;
        return true;
    case 0x52: // Up
        out->kind = KEY_UP;
        out->len = 0;
        return true;
    default:
        break;
    }

    // emacs-style: M-f forward word, M-b backward word
    if (mods & MOD_ALT) {
        if (keycode == 0x09) { // f
            out->kind = KEY_WORD_RIGHT;
            out->len = 0;
            return true;
        }
        if (keycode == 0x05) { // b
            out->kind = KEY_WORD_LEFT;
            out->len = 0;
            return true;
        }
        return false; // swallow other Alt+letter so it doesn't type
    }

    // emacs-style: ctrl-a line start, ctrl-e line end
    if (mods & MOD_CTRL) {
        if (keycode == 0x04) { // a
            out->kind = KEY_LINE_START;
            out->len = 0;
            return true;
        }
        if (keycode == 0x08) { // e
            out->kind = KEY_LINE_END;
            out->len = 0;
            return true;
        }
        if (keycode == 0x13) { // p
            out->kind = KEY_UP;
            out->len = 0;
            return true;
        }
        if (keycode == 0x11) { // n
            out->kind = KEY_DOWN;
            out->len = 0;
            return true;
        }
        if (keycode == 0x05) { // b
            out->kind = KEY_LEFT;
            out->len = 0;
            return true;
        }
        if (keycode == 0x09) { // f
            out->kind = KEY_RIGHT;
            out->len = 0;
            return true;
        }
        if (keycode == 0x1D) { // z: undo
            out->kind = KEY_UNDO;
            out->len = 0;
            return true;
        }
        if (keycode == 0x1C) { // y: redo
            out->kind = KEY_REDO;
            out->len = 0;
            return true;
        }
        return false; // swallow other Ctrl+letter so it doesn't type
    }

    if (keycode < sizeof(unshifted)) {
        bool shift = (mods & MOD_SHIFT) != 0;
        // caps lock inverts shift for letters only. digits and punctuation
        // are unaffected (shift+1 is '!' regardless of caps lock).
        if (caps_lock && keycode >= 0x04 && keycode <= 0x1D) {
            shift = !shift;
        }
        char c = shift ? shifted[keycode] : unshifted[keycode];
        if (c != 0) {
            out->kind = KEY_INSERT;
            out->bytes[0] = c;
            out->len = 1;
            return true;
        }
    }
    return false;
}
