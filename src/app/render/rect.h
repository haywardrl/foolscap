#pragma once

#include <stdbool.h>

typedef struct {
    int x, y, w, h;
} rect_t;

// empty if it has no area
bool rect_is_empty(rect_t r);

// bounding box of both
rect_t rect_union(rect_t a, rect_t b);
