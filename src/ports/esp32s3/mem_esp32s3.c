// #include "esp_heap_caps.h"
// #include "hal/hal_mem.h"
//
// #include <stdlib.h>
//
// void *hal_mem_alloc(size_t size, hal_mem_hint_t hint) {
//     if (hint == HAL_MEM_LARGE) {
//         return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
//     }
//     return malloc(size); // ESP-IDF's malloc uses internal SRAM by default
// }
//
// void hal_mem_free(void *ptr) {
//     free(ptr); // Works for both internal and PSRAM allocations
// }
