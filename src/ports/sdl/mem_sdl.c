#include "hal/hal_mem.h"

#include <stdlib.h>

void *hal_mem_alloc(size_t size, hal_mem_hint_t hint) {
    (void)hint; // SDL has one heap; hint is informational only
    return malloc(size);
}

void hal_mem_free(void *ptr) {
    free(ptr);
}
