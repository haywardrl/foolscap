#include "rect.h"

bool rect_is_empty(rect_t r) {
    return r.w <= 0 || r.h <= 0;
}

rect_t rect_union(rect_t a, rect_t b) {
    if (rect_is_empty(a))
        return b;
    if (rect_is_empty(b))
        return a;

    int x0 = a.x < b.x ? a.x : b.x;
    int y0 = a.y < b.y ? a.y : b.y;
    int ax1 = a.x + a.w, bx1 = b.x + b.w;
    int ay1 = a.y + a.h, by1 = b.y + b.h;
    int x1 = ax1 > bx1 ? ax1 : bx1;
    int y1 = ay1 > by1 ? ay1 : by1;
    return (rect_t){x0, y0, x1 - x0, y1 - y0};
}
