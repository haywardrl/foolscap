#include "buffer.h"

#include <stdlib.h>

struct buffer {
    char *data;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
};

// create buffer: must be a pointer as buffer_t is opaque
buffer_t *buffer_create(size_t capacity) {

    // allocate memory for a new buffer
    buffer_t *buffer = malloc(sizeof(*buffer));
    if (buffer == NULL) {
        return NULL;
    }

    // allocate memory to data the size of buffer capacity
    if ((buffer->data = malloc(capacity)) == NULL) {
        free(buffer);
        return NULL;
    }
    buffer->capacity = capacity;
    buffer->gap_start = 0;
    buffer->gap_end = capacity;

    return buffer;
}

// destroy buffer: free the data malloc first then the buffer_t
void buffer_destroy(buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    free(buffer);
}
