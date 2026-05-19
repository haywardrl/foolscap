#pragma once
#include <stddef.h>

typedef enum {
    HAL_MEM_DEFAULT, // SRAM
    HAL_MEM_LARGE,   // PSRAM
} hal_mem_hint_t;

void *hal_mem_alloc(size_t size, hal_mem_hint_t hint);
void hal_mem_free(void *ptr);
