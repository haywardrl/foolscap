#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

unsigned char *read_file(const char *filename, size_t *size);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fontgen <path-to-ttf>\n");
        return 1;
    }

    size_t size;
    const char *filename = argv[1];
    unsigned char *buffer = read_file(filename, &size);
    if (buffer == NULL) {
        return 1;
    }

    printf("Read %zu bytes\n", size);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0))) {
        fprintf(stderr, "Failed to initialize font\n");
        free(buffer);
        return 1;
    }

    int ascent;
    int descent;
    int line_gap;

    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    printf("Ascent: %d\n", ascent);
    printf("Descent %d\n", descent);
    printf("Line Gap: %d\n", line_gap);

    free(buffer);
    return 0;
}

unsigned char *read_file(const char *filename, size_t *size) {

    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: could not open '%s': %s\n", filename, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 0) {
        fprintf(stderr, "Error: could not find file_size '%s': %s\n", filename, strerror(errno));
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    size_t buffer_size = (size_t)file_size;

    unsigned char *buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Error: could not malloc buffer\n");
        fclose(f);
        return NULL;
    }

    const size_t read_bytes = fread(buffer, 1, buffer_size, f);
    if (read_bytes != buffer_size) {
        fprintf(stderr, "Error: not all bytes read into buffer\n");
        fclose(f);
        free(buffer);
        return NULL;
    }
    fclose(f);
    *size = buffer_size;
    return buffer;
}
