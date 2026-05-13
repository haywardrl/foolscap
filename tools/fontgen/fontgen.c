#include "font.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define INITIAL_BITMAP_CAPACITY (4 * 1024)
#define INITIAL_GLYPH_CAPACITY 128

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} byte_buffer_t;

typedef struct {
    font_glyph_t *data;
    size_t size;
    size_t capacity;
} glyph_buffer_t;

typedef struct {
    uint32_t start;
    uint32_t end; /* inclusive */
} codepoint_range_t;

typedef struct {
    codepoint_range_t *ranges;
    size_t count;
} range_list_t;

/* Buffer helpers */
static int byte_buffer_init(byte_buffer_t *b, size_t capacity);
static int byte_buffer_append(byte_buffer_t *b, const uint8_t *src, size_t n);
static void byte_buffer_free(byte_buffer_t *b);

static int glyph_buffer_init(glyph_buffer_t *g, size_t capacity);
static int glyph_buffer_append(glyph_buffer_t *g, font_glyph_t entry);
static void glyph_buffer_free(glyph_buffer_t *g);

/* Range parsing */
static int parse_ranges(const char *spec, range_list_t *out);
static void range_list_free(range_list_t *r);

/* File I/O */
static unsigned char *read_file(const char *filename, size_t *size);

/* Packing */
static void pack_1bpp(const unsigned char *glyph, int w, int h, int threshold, uint8_t *out);

/* Sorting */
static int compare_glyphs_by_codepoint(const void *a, const void *b);

/* Emit */
static int emit_files(const char *basename, int argc, char *argv[], const char *font_name,
                      int pixel_size, int ascent, int descent, int line_height, bool is_monospace,
                      const glyph_buffer_t *glyphs, const byte_buffer_t *bitmap);
static void make_identifier(const char *basename, char *out, size_t out_size);
static const char *basename_trailing(const char *path);
static void emit_header_comment(FILE *f);

#ifdef DEBUG_ATLAS
typedef struct {
    unsigned char *pixels;
    int w;
    int h;
} Atlas;
static Atlas build_atlas(const stbtt_fontinfo *font);
static int write_pgm(const char *path, const unsigned char *pixels, int w, int h);
static void copy_into_atlas(unsigned char *atlas, int atlas_w, const unsigned char *glyph, int w,
                            int h, int dest_x, int dest_y);
#endif

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: fontgen <path-to-ttf> <pixel-size> <ranges> <output-basename>\n"
                        "  ranges: comma-separated codepoints/ranges, hex or decimal\n"
                        "  output-basename: path without extension, .c and .h are appended\n"
                        "  example: fontgen IBMPlexMono.ttf 20 0x20-0x7E "
                        "src/app/render/fonts/ibm_plex_mono_20\n");
        return 1;
    }

    const char *ttf_path = argv[1];
    const char *output_basename = argv[4];

    size_t file_size;
    unsigned char *buffer = read_file(ttf_path, &file_size);
    if (buffer == NULL)
        return 1;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0))) {
        fprintf(stderr, "Failed to initialize font\n");
        free(buffer);
        return 1;
    }

    const float font_size = strtof(argv[2], NULL);
    if (font_size <= 0.0f) {
        fprintf(stderr, "Error: invalid pixel size '%s'\n", argv[2]);
        free(buffer);
        return 1;
    }

    range_list_t ranges = {0};
    if (parse_ranges(argv[3], &ranges) != 0) {
        free(buffer);
        return 1;
    }

    const int threshold = 128;
    const float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent_raw, descent_raw, line_gap_raw;
    stbtt_GetFontVMetrics(&font, &ascent_raw, &descent_raw, &line_gap_raw);
    const int ascent = (int)(ascent_raw * scale + 0.5f);
    const int descent = (int)(descent_raw * scale - 0.5f); /* negative, rounds away from zero */
    const int line_height = (int)((ascent_raw - descent_raw + line_gap_raw) * scale + 0.5f);

    byte_buffer_t bitmap_buf;
    glyph_buffer_t glyph_buf;
    if (byte_buffer_init(&bitmap_buf, INITIAL_BITMAP_CAPACITY) != 0 ||
        glyph_buffer_init(&glyph_buf, INITIAL_GLYPH_CAPACITY) != 0) {
        fprintf(stderr, "Error: could not allocate staging buffers\n");
        byte_buffer_free(&bitmap_buf);
        glyph_buffer_free(&glyph_buf);
        range_list_free(&ranges);
        free(buffer);
        return 1;
    }

    for (size_t r = 0; r < ranges.count; r++) {
        for (uint32_t cp = ranges.ranges[r].start; cp <= ranges.ranges[r].end; cp++) {
            int advance_width_unscaled, lsb_unscaled;
            stbtt_GetCodepointHMetrics(&font, (int)cp, &advance_width_unscaled, &lsb_unscaled);
            const int x_advance = (int)(advance_width_unscaled * scale + 0.5f);

            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char *glyph =
                stbtt_GetCodepointBitmap(&font, 0, scale, (int)cp, &w, &h, &xoff, &yoff);

            uint32_t this_glyph_offset = (uint32_t)bitmap_buf.size;

            if (glyph != NULL && w > 0 && h > 0) {
                const int stride = (w + 7) / 8;
                const size_t packed_size = (size_t)stride * (size_t)h;

                uint8_t *staging = malloc(packed_size);
                if (!staging) {
                    fprintf(stderr, "Error: could not allocate staging for cp 0x%04x\n", cp);
                    stbtt_FreeBitmap(glyph, NULL);
                    goto fail;
                }
                pack_1bpp(glyph, w, h, threshold, staging);
                if (byte_buffer_append(&bitmap_buf, staging, packed_size) != 0) {
                    fprintf(stderr, "Error: could not append bitmap for cp 0x%04x\n", cp);
                    free(staging);
                    stbtt_FreeBitmap(glyph, NULL);
                    goto fail;
                }
                free(staging);
            }

            assert(xoff >= INT8_MIN && xoff <= INT8_MAX);
            assert(yoff >= INT8_MIN && yoff <= INT8_MAX);
            assert(x_advance >= 0 && x_advance <= UINT8_MAX);
            assert(w >= 0 && w <= UINT8_MAX);
            assert(h >= 0 && h <= UINT8_MAX);

            font_glyph_t entry = {
                .codepoint = cp,
                .bitmap_offset = this_glyph_offset,
                .width = (uint8_t)w,
                .height = (uint8_t)h,
                .x_offset = (int8_t)xoff,
                .y_offset = (int8_t)yoff,
                .x_advance = (uint8_t)x_advance,
            };
            if (glyph_buffer_append(&glyph_buf, entry) != 0) {
                fprintf(stderr, "Error: could not append glyph entry for cp 0x%04x\n", cp);
                if (glyph)
                    stbtt_FreeBitmap(glyph, NULL);
                goto fail;
            }

            if (glyph)
                stbtt_FreeBitmap(glyph, NULL);
        }
    }

    /* Sort by codepoint so the renderer can binary-search regardless of input order. */
    qsort(glyph_buf.data, glyph_buf.size, sizeof(font_glyph_t), compare_glyphs_by_codepoint);

    /* Monospace detection: do all non-zero-width glyphs share the same x_advance? */
    bool is_monospace = true;
    uint8_t reference_advance = 0;
    bool reference_set = false;
    for (size_t i = 0; i < glyph_buf.size; i++) {
        const font_glyph_t *g = &glyph_buf.data[i];
        if (g->width == 0)
            continue;
        if (!reference_set) {
            reference_advance = g->x_advance;
            reference_set = true;
        } else if (g->x_advance != reference_advance) {
            is_monospace = false;
            break;
        }
    }

    const char *font_name = basename_trailing(output_basename);
    if (emit_files(output_basename, argc, argv, font_name, (int)font_size, ascent, descent,
                   line_height, is_monospace, &glyph_buf, &bitmap_buf) != 0) {
        goto fail;
    }

    printf("Wrote %s.c and %s.h\n", output_basename, output_basename);
    printf("  glyphs: %zu  bitmap: %zu bytes  monospace: %s\n", glyph_buf.size, bitmap_buf.size,
           is_monospace ? "yes" : "no");

    byte_buffer_free(&bitmap_buf);
    glyph_buffer_free(&glyph_buf);
    range_list_free(&ranges);
    free(buffer);
    return 0;

fail:
    byte_buffer_free(&bitmap_buf);
    glyph_buffer_free(&glyph_buf);
    range_list_free(&ranges);
    free(buffer);
    return 1;
}

/* ---------- byte_buffer_t ---------- */

static int byte_buffer_init(byte_buffer_t *b, size_t capacity) {
    b->data = malloc(capacity);
    if (!b->data) {
        b->size = b->capacity = 0;
        return -1;
    }
    b->size = 0;
    b->capacity = capacity;
    return 0;
}

static int byte_buffer_append(byte_buffer_t *b, const uint8_t *src, size_t n) {
    if (b->size + n > b->capacity) {
        size_t new_capacity = b->capacity ? b->capacity : 1;
        while (new_capacity < b->size + n)
            new_capacity *= 2;
        uint8_t *grown = realloc(b->data, new_capacity);
        if (!grown)
            return -1;
        b->data = grown;
        b->capacity = new_capacity;
    }
    memcpy(b->data + b->size, src, n);
    b->size += n;
    return 0;
}

static void byte_buffer_free(byte_buffer_t *b) {
    free(b->data);
    b->data = NULL;
    b->size = b->capacity = 0;
}

/* ---------- glyph_buffer_t ---------- */

static int glyph_buffer_init(glyph_buffer_t *g, size_t capacity) {
    g->data = malloc(capacity * sizeof(font_glyph_t));
    if (!g->data) {
        g->size = g->capacity = 0;
        return -1;
    }
    g->size = 0;
    g->capacity = capacity;
    return 0;
}

static int glyph_buffer_append(glyph_buffer_t *g, font_glyph_t entry) {
    if (g->size + 1 > g->capacity) {
        size_t new_capacity = g->capacity ? g->capacity * 2 : 1;
        font_glyph_t *grown = realloc(g->data, new_capacity * sizeof(font_glyph_t));
        if (!grown)
            return -1;
        g->data = grown;
        g->capacity = new_capacity;
    }
    g->data[g->size++] = entry;
    return 0;
}

static void glyph_buffer_free(glyph_buffer_t *g) {
    free(g->data);
    g->data = NULL;
    g->size = g->capacity = 0;
}

/* ---------- range parsing ---------- */

static int parse_ranges(const char *spec, range_list_t *out) {
    out->ranges = NULL;
    out->count = 0;

    size_t max_ranges = 1;
    for (const char *p = spec; *p; p++) {
        if (*p == ',')
            max_ranges++;
    }

    out->ranges = malloc(max_ranges * sizeof(codepoint_range_t));
    if (!out->ranges)
        return -1;

    const char *p = spec;
    while (*p) {
        while (isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;

        char *endp;
        unsigned long start = strtoul(p, &endp, 0);
        if (endp == p) {
            fprintf(stderr, "Error: could not parse codepoint at '%s'\n", p);
            range_list_free(out);
            return -1;
        }
        p = endp;
        unsigned long end = start;

        while (isspace((unsigned char)*p))
            p++;
        if (*p == '-') {
            p++;
            while (isspace((unsigned char)*p))
                p++;
            end = strtoul(p, &endp, 0);
            if (endp == p) {
                fprintf(stderr, "Error: could not parse range end at '%s'\n", p);
                range_list_free(out);
                return -1;
            }
            p = endp;
        }

        if (end < start) {
            fprintf(stderr, "Error: range end 0x%lx before start 0x%lx\n", end, start);
            range_list_free(out);
            return -1;
        }
        if (end > 0x10FFFF) {
            fprintf(stderr, "Error: codepoint 0x%lx out of Unicode range\n", end);
            range_list_free(out);
            return -1;
        }

        out->ranges[out->count].start = (uint32_t)start;
        out->ranges[out->count].end = (uint32_t)end;
        out->count++;

        while (isspace((unsigned char)*p))
            p++;
        if (*p == ',')
            p++;
    }

    if (out->count == 0) {
        fprintf(stderr, "Error: no codepoint ranges parsed\n");
        range_list_free(out);
        return -1;
    }
    return 0;
}

static void range_list_free(range_list_t *r) {
    free(r->ranges);
    r->ranges = NULL;
    r->count = 0;
}

/* ---------- file I/O ---------- */

static unsigned char *read_file(const char *filename, size_t *size) {
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

/* ---------- packing ---------- */

static void pack_1bpp(const unsigned char *glyph, int w, int h, int threshold, uint8_t *out) {
    const int src_stride = w;
    const int dst_stride = (w + 7) / 8;
    memset(out, 0, (size_t)dst_stride * (size_t)h);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (glyph[j * src_stride + i] > threshold) {
                out[j * dst_stride + (i / 8)] |= (uint8_t)(1 << (7 - (i % 8)));
            }
        }
    }
}

/* ---------- sorting ---------- */

static int compare_glyphs_by_codepoint(const void *a, const void *b) {
    const font_glyph_t *ga = (const font_glyph_t *)a;
    const font_glyph_t *gb = (const font_glyph_t *)b;
    if (ga->codepoint < gb->codepoint)
        return -1;
    if (ga->codepoint > gb->codepoint)
        return 1;
    return 0;
}

/* ---------- emit helpers ---------- */

/* Find the last '/' (or '\\' on Windows-ish paths) and return everything after.
 * If none, returns the whole string. */
static const char *basename_trailing(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            last = p + 1;
    }
    return last;
}

/* Build a valid C identifier from a basename: uppercase, non-alphanumeric → '_'. */
static void make_identifier(const char *basename, char *out, size_t out_size) {
    const char *trailing = basename_trailing(basename);
    size_t i = 0;
    for (; trailing[i] && i + 1 < out_size; i++) {
        char c = trailing[i];
        if (isalnum((unsigned char)c)) {
            out[i] = (char)toupper((unsigned char)c);
        } else {
            out[i] = '_';
        }
    }
    out[i] = '\0';
}

/* ---------- emit ---------- */

static int emit_files(const char *basename, int argc, char *argv[], const char *font_name,
                      int pixel_size, int ascent, int descent, int line_height, bool is_monospace,
                      const glyph_buffer_t *glyphs, const byte_buffer_t *bitmap) {
    char ident[128];
    make_identifier(basename, ident, sizeof(ident));

    char c_path[512];
    char h_path[512];
    if (snprintf(c_path, sizeof(c_path), "%s.c", basename) >= (int)sizeof(c_path) ||
        snprintf(h_path, sizeof(h_path), "%s.h", basename) >= (int)sizeof(h_path)) {
        fprintf(stderr, "Error: output basename too long\n");
        return -1;
    }

    const char *trailing_name = basename_trailing(basename);

    /* ---- emit .h ---- */
    FILE *fh = fopen(h_path, "w");
    if (!fh) {
        fprintf(stderr, "Error: could not open '%s' for writing: %s\n", h_path, strerror(errno));
        return -1;
    }
    fprintf(fh, "/* Generated by fontgen. Do not edit. */\n");
    fprintf(fh, "/* Regenerate with:\n *   ");
    for (int i = 0; i < argc; i++) {
        fprintf(fh, "%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    fprintf(fh, "\n */\n\n");
    emit_header_comment(fh);
    fprintf(fh, "\n");
    fprintf(fh, "#pragma once\n\n");
    fprintf(fh, "#include \"font.h\"\n\n");
    fprintf(fh, "extern const font_t %s;\n", ident);
    if (fclose(fh) != 0) {
        fprintf(stderr, "Error: could not close '%s'\n", h_path);
        return -1;
    }

    /* ---- emit .c ---- */
    FILE *fc = fopen(c_path, "w");
    if (!fc) {
        fprintf(stderr, "Error: could not open '%s' for writing: %s\n", c_path, strerror(errno));
        return -1;
    }
    fprintf(fc, "/* Generated by fontgen. Do not edit. */\n");
    fprintf(fc, "\n\n");
    fprintf(fc, "/* Regenerate with:\n *   ");
    for (int i = 0; i < argc; i++) {
        fprintf(fc, "%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    fprintf(fc, "\n */\n\n");
    emit_header_comment(fc);
    fprintf(fc, "#include \"%s.h\"\n\n", trailing_name);

    /* Bitmap data */
    fprintf(fc, "static const uint8_t %s_BITMAP[] = {\n", ident);
    for (size_t i = 0; i < bitmap->size; i++) {
        if (i % 12 == 0)
            fprintf(fc, "    ");
        fprintf(fc, "0x%02x,", bitmap->data[i]);
        if ((i + 1) % 12 == 0 || i + 1 == bitmap->size) {
            fprintf(fc, "\n");
        } else {
            fprintf(fc, " ");
        }
    }
    fprintf(fc, "};\n\n");

    /* Glyph table */
    fprintf(fc, "static const font_glyph_t %s_GLYPHS[] = {\n", ident);
    for (size_t i = 0; i < glyphs->size; i++) {
        const font_glyph_t *g = &glyphs->data[i];
        char ch = (g->codepoint >= 0x20 && g->codepoint <= 0x7E) ? (char)g->codepoint : '?';
        fprintf(fc,
                "    { .codepoint = 0x%04x, .bitmap_offset = %u, .width = %u, .height = %u, "
                ".x_offset = %d, .y_offset = %d, .x_advance = %u }, /* '%c' */\n",
                g->codepoint, g->bitmap_offset, g->width, g->height, g->x_offset, g->y_offset,
                g->x_advance, ch);
    }
    fprintf(fc, "};\n\n");

    /* font_t instance */
    fprintf(fc, "const font_t %s = {\n", ident);
    fprintf(fc, "    .name = \"%s\",\n", font_name);
    fprintf(fc, "    .pixel_size = %d,\n", pixel_size);
    fprintf(fc, "    .line_height = %d,\n", line_height);
    fprintf(fc, "    .ascent = %d,\n", ascent);
    fprintf(fc, "    .descent = %d,\n", descent);
    fprintf(fc, "    .glyph_count = %zu,\n", glyphs->size);
    fprintf(fc, "    .is_monospace = %s,\n", is_monospace ? "true" : "false");
    fprintf(fc, "    .glyphs = %s_GLYPHS,\n", ident);
    fprintf(fc, "    .bitmap_data = %s_BITMAP,\n", ident);
    fprintf(fc, "    .bitmap_data_size = sizeof(%s_BITMAP),\n", ident);
    fprintf(fc, "};\n");

    if (fclose(fc) != 0) {
        fprintf(stderr, "Error: could not close '%s'\n", c_path);
        return -1;
    }
    return 0;
}

/* Write the licence/attribution header to an output file. Same block
 * goes into both .h and .c so each file stands alone from a licensing
 * standpoint — the .c contains the derivative bitmap data, the .h is
 * unlikely to but costs nothing to annotate consistently. */
static void emit_header_comment(FILE *f) {
    fprintf(f, "/*\n");
    fprintf(f, " * ---------------------------------------------------------------------------\n");
    fprintf(f, " * The glyph bitmap data in this file is derived from IBM Plex Mono,\n");
    fprintf(f, " * Copyright 2017 IBM Corp. (https://github.com/IBM/plex)\n");
    fprintf(f, " *\n");
    fprintf(f, " * Licensed under the SIL Open Font License, Version 1.1.\n");
    fprintf(f, " * The full license text is included with the source font in\n");
    fprintf(f, " * assets/fonts/OFL.txt and is also available at:\n");
    fprintf(f, " *     https://scripts.sil.org/OFL\n");
    fprintf(f, " * ---------------------------------------------------------------------------\n");
    fprintf(f, " */\n\n");
}

#ifdef DEBUG_ATLAS
static int write_pgm(const char *path, const unsigned char *pixels, int w, int h) {
    FILE *f = fopen(path, "wb");
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(pixels, 1, (size_t)(w * h), f);
    fclose(f);
    return 0;
}

static Atlas build_atlas(const stbtt_fontinfo *font) {
    static const int cell_w = 24;
    static const int cell_h = 28;
    static const int cols = 16;
    static const int rows = 6;

    Atlas result = {
        .pixels = NULL,
        .w = cols * cell_w,
        .h = rows * cell_h,
    };

    result.pixels = calloc((size_t)(result.w * result.h), 1);
    if (!result.pixels) {
        fprintf(stderr, "Error: could not allocate atlas\n");
        return result;
    }

    float scale = stbtt_ScaleForPixelHeight(font, 20);

    for (int cp = 0x20; cp <= 0x7E; cp++) {
        int w, h, xoff, yoff;
        unsigned char *glyph = stbtt_GetCodepointBitmap(font, 0, scale, cp, &w, &h, &xoff, &yoff);
        if (glyph == NULL)
            continue;
        if (w > cell_w || h > cell_h) {
            fprintf(stderr, "Glyph 0x%02x too big for cell: %dx%d\n", cp, w, h);
            stbtt_FreeBitmap(glyph, NULL);
            continue;
        }
        int idx = cp - 0x20;
        int col = idx % cols;
        int row = idx / cols;
        copy_into_atlas(result.pixels, result.w, glyph, w, h, col * cell_w, row * cell_h);
        stbtt_FreeBitmap(glyph, NULL);
    }
    return result;
}

static void copy_into_atlas(unsigned char *atlas, int atlas_w, const unsigned char *glyph, int w,
                            int h, int dest_x, int dest_y) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            atlas[(dest_y + j) * atlas_w + (dest_x + i)] = glyph[j * w + i];
        }
    }
}
#endif
