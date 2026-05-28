#include "hal/hal_mem.h"

#include <esp_heap_caps.h>
#include <stdlib.h>

void *hal_mem_alloc(size_t size, hal_mem_hint_t hint) {
    if (hint == HAL_MEM_LARGE) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    return malloc(size);
}

void hal_mem_free(void *ptr) {
    free(ptr);
}
