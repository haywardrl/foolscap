#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // Treat as opaque. Use utf8_iter_init.
    const char *p;
    const char *end;
} utf8_iter_t;

typedef enum {
    UTF8_OK,
    UTF8_END,
    UTF8_INVALID,
} utf8_status_t;

void utf8_iter_init(utf8_iter_t *it, const char *bytes, size_t len);
utf8_status_t utf8_next(utf8_iter_t *it, uint32_t *codepoint_out);
