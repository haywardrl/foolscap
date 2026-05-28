#include "app/buffer/buffer.h"
#include "app/layout/layout.h"
#include "app/render/font.h"

#include <stdio.h>
#include <string.h>
#include <unity.h>

// x_advance=10, line_height=20; layout only reads glyphs[0].
static const font_glyph_t glyphs[] = {{.codepoint = 'A', .x_advance = 10}};
static const font_t font = {.glyphs = glyphs, .glyph_count = 1, .line_height = 20};

#define WRAP 50 // 5 glyphs per line at x_advance=10
#define MX 0
#define MY 0

// the patch produced by the most recent check_patch, for tests that assert on
// which rows were reported as changed.
static layout_patch_t last_patch;

void setUp(void) {
    last_patch = (layout_patch_t){0};
}
void tearDown(void) {
}

static void assert_layouts_equal(const layout_t *expected, const layout_t *actual) {
    TEST_ASSERT_EQUAL_size_t_MESSAGE(expected->count, actual->count, "line count");
    for (size_t i = 0; i < expected->count; i++) {
        char msg[64];
        snprintf(msg, sizeof msg, "line %zu byte_start", i);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(expected->lines[i].byte_start, actual->lines[i].byte_start,
                                         msg);
        snprintf(msg, sizeof msg, "line %zu byte_end", i);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(expected->lines[i].byte_end, actual->lines[i].byte_end,
                                         msg);
        snprintf(msg, sizeof msg, "line %zu y", i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(expected->lines[i].y, actual->lines[i].y, msg);
        snprintf(msg, sizeof msg, "line %zu ends_with_newline", i);
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)expected->lines[i].ends_with_newline,
                                      (int)actual->lines[i].ends_with_newline, msg);
    }
}

// patching the pre-edit layout must match a fresh compute of the post-edit text.
// the buffer holds `after` with the gap at the cursor, so the edited line
// straddles the gap as it would in the editor.
static void check_patch(const char *before, edit_t edit, const char *after) {
    layout_t patched;
    layout_t expected;

    buffer_t *buf = buffer_create(64);
    buffer_insert_bytes(buf, after, strlen(after));
    size_t cursor = (edit.type == EDIT_INSERT) ? edit.pos + edit.len : edit.pos;
    buffer_set_cursor(buf, cursor);

    char scratch[64];
    TEST_ASSERT_TRUE(layout_compute(&patched, before, strlen(before), &font, WRAP, MX, MY));
    TEST_ASSERT_TRUE(layout_apply_edit(&patched, edit, buf, scratch, sizeof(scratch), &last_patch));
    TEST_ASSERT_TRUE(layout_compute(&expected, after, strlen(after), &font, WRAP, MX, MY));

    assert_layouts_equal(&expected, &patched);

    layout_destroy(&patched);
    layout_destroy(&expected);
    buffer_destroy(buf);
}

static void test_insert_char_midline(void) {
    check_patch("AB", (edit_t){.type = EDIT_INSERT, .pos = 1, .len = 1}, "AXB");
}

static void test_insert_char_at_end(void) {
    check_patch("AB", (edit_t){.type = EDIT_INSERT, .pos = 2, .len = 1}, "ABC");
}

static void test_insert_into_empty(void) {
    check_patch("", (edit_t){.type = EDIT_INSERT, .pos = 0, .len = 1}, "X");
}

static void test_delete_char_midline(void) {
    check_patch("ABC", (edit_t){.type = EDIT_DELETE, .pos = 1, .len = 1}, "AC");
}

static void test_delete_to_empty(void) {
    check_patch("A", (edit_t){.type = EDIT_DELETE, .pos = 0, .len = 1}, "");
}

static void test_insert_causes_wrap(void) {
    // "ABCD" + "EFG" at end -> "ABCDEFG" wraps at 5 glyphs into [0,5) [5,7)
    check_patch("ABCD", (edit_t){.type = EDIT_INSERT, .pos = 4, .len = 3}, "ABCDEFG");
}

static void test_insert_newline_splits_line(void) {
    check_patch("ABCD", (edit_t){.type = EDIT_INSERT, .pos = 2, .len = 1}, "AB\nCD");
}

static void test_insert_newline_midword_shifts_lines_below(void) {
    // split line 0; the existing second line must shift +1 byte and +1 row
    check_patch("AB\nCD", (edit_t){.type = EDIT_INSERT, .pos = 1, .len = 1}, "A\nB\nCD");
}

static void test_insert_newline_at_line_start(void) {
    // Enter at start of line -> empty line above
    check_patch("AB", (edit_t){.type = EDIT_INSERT, .pos = 0, .len = 1}, "\nAB");
}

static void test_insert_newline_at_line_end(void) {
    // Enter before an existing newline -> empty line in the middle
    check_patch("AB\nCD", (edit_t){.type = EDIT_INSERT, .pos = 2, .len = 1}, "AB\n\nCD");
}

static void test_insert_newline_in_soft_wrapped_line(void) {
    // "ABCDEFG" wraps into [0,5)[5,7); split the second (soft-wrapped) piece
    check_patch("ABCDEFG", (edit_t){.type = EDIT_INSERT, .pos = 6, .len = 1}, "ABCDEF\nG");
}

static void test_delete_merges_lines(void) {
    // remove the newline, two lines collapse to one
    check_patch("AB\nCD", (edit_t){.type = EDIT_DELETE, .pos = 2, .len = 1}, "ABCD");
}

static void test_delete_merge_shifts_lines_below(void) {
    // merge lines 0 and 1; the third line must shift -1 byte and -1 row
    check_patch("A\nB\nCD", (edit_t){.type = EDIT_DELETE, .pos = 1, .len = 1}, "AB\nCD");
}

static void test_delete_merge_overflows_falls_back(void) {
    // "ABCD" + "EFG" = 7 glyphs > WRAP(5), so the merge re-wraps -> fallback
    check_patch("ABCD\nEFG", (edit_t){.type = EDIT_DELETE, .pos = 4, .len = 1}, "ABCDEFG");
}

static void test_delete_merge_into_empty_trailing_line(void) {
    // trailing newline: deleting it merges the empty final line away
    check_patch("AB\n", (edit_t){.type = EDIT_DELETE, .pos = 2, .len = 1}, "AB");
}

static void test_delete_merge_with_soft_wrapped_next(void) {
    // "A\nBCDEFG": line1 "BCDEFG" soft-wraps; merging keeps the wrap below
    check_patch("A\nBCDEFG", (edit_t){.type = EDIT_DELETE, .pos = 1, .len = 1}, "ABCDEFG");
}

// patch reporting: which rows a caller must repaint.

static void test_patch_inline_insert_reports_single_row(void) {
    check_patch("AB", (edit_t){.type = EDIT_INSERT, .pos = 1, .len = 1}, "AXB");
    TEST_ASSERT_FALSE(last_patch.recomputed);
    TEST_ASSERT_FALSE(last_patch.to_end);
    TEST_ASSERT_EQUAL_size_t(0, last_patch.first_line);
    TEST_ASSERT_EQUAL_size_t(0, last_patch.last_line);
}

static void test_patch_inline_insert_second_line_reports_that_row(void) {
    check_patch("A\nBC", (edit_t){.type = EDIT_INSERT, .pos = 3, .len = 1}, "A\nBXC");
    TEST_ASSERT_FALSE(last_patch.recomputed);
    TEST_ASSERT_FALSE(last_patch.to_end);
    TEST_ASSERT_EQUAL_size_t(1, last_patch.first_line);
    TEST_ASSERT_EQUAL_size_t(1, last_patch.last_line);
}

static void test_patch_newline_split_reports_to_end(void) {
    check_patch("AB\nCD", (edit_t){.type = EDIT_INSERT, .pos = 1, .len = 1}, "A\nB\nCD");
    TEST_ASSERT_FALSE(last_patch.recomputed);
    TEST_ASSERT_TRUE(last_patch.to_end);
    TEST_ASSERT_EQUAL_size_t(0, last_patch.first_line);
}

static void test_patch_merge_reports_to_end(void) {
    check_patch("A\nB\nCD", (edit_t){.type = EDIT_DELETE, .pos = 1, .len = 1}, "AB\nCD");
    TEST_ASSERT_FALSE(last_patch.recomputed);
    TEST_ASSERT_TRUE(last_patch.to_end);
    TEST_ASSERT_EQUAL_size_t(0, last_patch.first_line);
}

static void test_patch_wrap_fallback_reports_recomputed(void) {
    check_patch("ABCD", (edit_t){.type = EDIT_INSERT, .pos = 4, .len = 3}, "ABCDEFG");
    TEST_ASSERT_TRUE(last_patch.recomputed);
}

// a multi-line insert (paste, storage load) must lay out like a fresh
// compute. fast single-newline split can't handle it.
static void test_insert_multiline_into_empty(void) {
    check_patch("", (edit_t){.type = EDIT_INSERT, .pos = 0, .len = 5}, "A\nB\nC");
}

static void test_insert_multibyte_with_newline(void) {
    check_patch("AB", (edit_t){.type = EDIT_INSERT, .pos = 2, .len = 3}, "AB\nXY");
}

static void test_patch_multiline_insert_reports_recomputed(void) {
    check_patch("", (edit_t){.type = EDIT_INSERT, .pos = 0, .len = 5}, "A\nB\nC");
    TEST_ASSERT_TRUE(last_patch.recomputed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_patch_inline_insert_reports_single_row);
    RUN_TEST(test_patch_inline_insert_second_line_reports_that_row);
    RUN_TEST(test_patch_newline_split_reports_to_end);
    RUN_TEST(test_patch_merge_reports_to_end);
    RUN_TEST(test_patch_wrap_fallback_reports_recomputed);
    RUN_TEST(test_insert_multiline_into_empty);
    RUN_TEST(test_insert_multibyte_with_newline);
    RUN_TEST(test_patch_multiline_insert_reports_recomputed);
    RUN_TEST(test_insert_char_midline);
    RUN_TEST(test_insert_char_at_end);
    RUN_TEST(test_insert_into_empty);
    RUN_TEST(test_delete_char_midline);
    RUN_TEST(test_delete_to_empty);
    RUN_TEST(test_insert_causes_wrap);
    RUN_TEST(test_insert_newline_splits_line);
    RUN_TEST(test_insert_newline_midword_shifts_lines_below);
    RUN_TEST(test_insert_newline_at_line_start);
    RUN_TEST(test_insert_newline_at_line_end);
    RUN_TEST(test_insert_newline_in_soft_wrapped_line);
    RUN_TEST(test_delete_merges_lines);
    RUN_TEST(test_delete_merge_shifts_lines_below);
    RUN_TEST(test_delete_merge_overflows_falls_back);
    RUN_TEST(test_delete_merge_into_empty_trailing_line);
    RUN_TEST(test_delete_merge_with_soft_wrapped_next);
    return UNITY_END();
}
