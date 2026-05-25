#include "utf8.h"

#include <assert.h>
#include <stdint.h>

void utf8_iter_init(utf8_iter_t *it, const char *bytes, size_t len) {
    assert(it != NULL);
    assert(bytes != NULL || len == 0); // NULL bytes Ok if len is 0
    it->p = bytes;
    it->end = bytes + len;
}

utf8_status_t utf8_next(utf8_iter_t *it, uint32_t *codepoint_out) {
    if (it->p >= it->end) {
        return UTF8_END;
    }

    /*
     *0xxxxxxx  →  1-byte sequence (ASCII), payload in low 7 bits
     *110xxxxx  →  2-byte sequence, payload in low 5 bits
     *1110xxxx  →  3-byte sequence, payload in low 4 bits
     *11110xxx  →  4-byte sequence, payload in low 3 bits
     *10xxxxxx  →  INVALID as a lead byte (this is a continuation byte)
     *11111xxx  →  INVALID (no 5+ byte sequences in modern UTF-8)
     */

    /*
     *2-byte: must be ≥ 0x80
     *3-byte: must be ≥ 0x800
     *4-byte: must be ≥ 0x10000
     */

    uint8_t b0 = (uint8_t)*it->p;
    int seq_len;
    uint32_t cp;
    uint32_t min_codepoint;

    if (b0 < 0x80) {
        *codepoint_out = b0;
        it->p += 1;
        return UTF8_OK;
    }
    if ((b0 & 0x80) == 0x00) { // 1-byte: top bit is 0
        *codepoint_out = b0;
        it->p += 1;
        return UTF8_OK;
    } else if ((b0 & 0xE0) == 0xC0) { // 2-byte: top bits are 110
        seq_len = 2;
        cp = b0 & 0x1F;
        min_codepoint = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) { // 3-byte: top bits are 1110
        seq_len = 3;
        cp = b0 & 0x0F;
        min_codepoint = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) { // 4-byte: top bits are 11110
        seq_len = 4;
        cp = b0 & 0x07;
        min_codepoint = 0x10000;
    } else { /* invalid lead byte */
        it->p += 1;
        return UTF8_INVALID;
    }

    // Check enough bytes remaining
    if (it->p + seq_len > it->end) {
        it->p += 1;
        return UTF8_INVALID;
    }

    // Read continuation bytes
    for (int i = 1; i < seq_len; i++) {
        uint8_t bn = (uint8_t)it->p[i];
        if ((bn & 0xC0) != 0x80) { // check byte starts 10xx xxxx
            it->p += 1;
            return UTF8_INVALID;
        }
        cp = (cp << 6) | (bn & 0x3F);
    }

    if (cp < min_codepoint) {
        it->p += 1;
        return UTF8_INVALID; // overlong
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        it->p += 1;
        return UTF8_INVALID; // surrogate
    }
    if (cp > 0x10FFFF) {
        it->p += 1;
        return UTF8_INVALID; // above Unicode range
    }

    *codepoint_out = cp;
    it->p += seq_len;
    return UTF8_OK;
}

size_t utf8_count_codepoints(const char *bytes, size_t len) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (((unsigned char)bytes[i] & 0xC0) != 0x80)
            n++;
    }
    return n;
}

size_t utf8_advance_codepoints(const char *bytes, size_t len, size_t n) {
    size_t seen = 0;
    size_t i = 0;
    while (i < len && seen < n) {
        i++;
        // step past any continuation bytes
        while (i < len && ((unsigned char)bytes[i] & 0xC0) == 0x80)
            i++;
        seen++;
    }
    return i;
}
