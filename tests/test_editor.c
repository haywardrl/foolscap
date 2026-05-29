#include "app/buffer/buffer.h"
#include "app/editor/editor.h"
#include "app/editor/editor_config.h"
#include "app/editor/editor_test.h"
#include "app/render/font.h"
#include "hal/hal_display.h"
#include "mocks/display_mock.h"
#include "unity_internals.h"

#include <stdint.h>
#include <unity.h>

static const font_glyph_t glyphs[] = {
    {.codepoint = 'A', .x_advance = 10},
    {.codepoint = 'B', .x_advance = 7},
    {.codepoint = 0x20AC, .x_advance = 13}, // Euro
};
static const font_t font = {
    .glyphs = glyphs,
    .glyph_count = 3,
    .line_height = 20,
};

// ascent > 0 puts the cursor underline below the text row, which the
// ascent=0 font above cannot exercise.
static const font_t font_ascent = {
    .glyphs = glyphs,
    .glyph_count = 3,
    .line_height = 20,
    .ascent = 16,
};

void setUp(void) {
    display_mock_reset();
}

void tearDown(void) {
}

static void test_editor_create_returns_non_null_for_valid_args(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_NOT_NULL(ed);
    editor_destroy(ed);
}

static void test_editor_create_returns_null_for_null_font(void) {
    TEST_ASSERT_NULL(editor_create(NULL, 256));
}

static void test_editor_create_returns_null_for_zero_capacity(void) {
    TEST_ASSERT_NULL(editor_create(&font, 0));
}

static void test_editor_create_returns_null_for_font_with_no_glyphs(void) {
    static const font_t font_no_glyphs = {
        .glyphs = NULL,
        .glyph_count = 0,
        .line_height = 20,
    };
    TEST_ASSERT_NULL(editor_create(&font_no_glyphs, 256));
}

static void test_editor_destroy_null_safe(void) {
    editor_destroy(NULL); // Should not crash
}

static void test_editor_insert_utf8_updates_buffer(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_insert_utf8_sets_dirty(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL_size_t(1, buffer_cursor_pos(buf));
    TEST_ASSERT_TRUE(editor_is_dirty(ed));
    editor_destroy(ed);
}

static void test_editor_insert_utf8_overflow_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_FALSE(editor_insert_utf8(ed, "AB", 2));
    editor_destroy(ed);
}

static void test_editor_backspace_at_zero_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    const buffer_t *buf = editor_test_get_buffer(ed);
    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buf));
    TEST_ASSERT_FALSE(editor_backspace(ed));
    editor_destroy(ed);
}

static void test_editor_backspace_deletes_one_codepoint_ascii(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_TRUE(editor_backspace(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_backspace_deletes_one_codepoint_multibyte_euro(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3); // 3-byte insert (Euro)
    TEST_ASSERT_TRUE(editor_backspace(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_delete_forward_at_end_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_FALSE(editor_delete_forward(ed));
    editor_destroy(ed);
}

static void test_editor_delete_forward_deletes_one_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
    TEST_ASSERT_TRUE(editor_delete_forward(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));
    editor_destroy(ed);
}

static void test_editor_move_cursor_left_at_zero_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    editor_destroy(ed);
}

static void test_editor_move_cursor_right_at_end_returns_false(void) {
    editor_t *ed = editor_create(&font, 1);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_RIGHT));
    editor_destroy(ed);
}

static void test_editor_move_cursor_left_advances_by_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_move_cursor_right_advances_by_codepoint_multibyte(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_RIGHT));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(6, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_line_start_jumps_to_start_of_visual_line(void) {
    // "AAAAAAAA" wraps after 6 As (wrap_width=80, advance=10): line0 [0,6), line1 [6,8)
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AAAAAAAA", 8);
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(8, buffer_cursor_pos(buf));
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_START));
    TEST_ASSERT_EQUAL_size_t(6, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_line_end_lands_before_newline(void) {
    // "AA\nAA" — line0 ends with \n, byte_end=2 (the \n position itself)
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AA\nAA", 5);
    const buffer_t *buf = editor_test_get_buffer(ed);
    buffer_set_cursor((buffer_t *)buf, 0);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    TEST_ASSERT_EQUAL_size_t(2, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_line_end_backs_off_one_codepoint_on_soft_wrap(void) {
    // "AAAAAAAA": from pos 0 on line0 (soft-wrapped), C-e backs off byte_end=6 → 5.
    // pos 6 would visually render as col 0 of line1, which isn't end-of-this-line.
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AAAAAAAA", 8);
    const buffer_t *buf = editor_test_get_buffer(ed);
    buffer_set_cursor((buffer_t *)buf, 0);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    TEST_ASSERT_EQUAL_size_t(5, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_line_end_lands_at_buffer_end_on_final_line(void) {
    // last layout line has ends_with_newline=false but is_last_line=true: no back-off.
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AAA", 3);
    const buffer_t *buf = editor_test_get_buffer(ed);
    buffer_set_cursor((buffer_t *)buf, 0);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    TEST_ASSERT_EQUAL_size_t(3, buffer_cursor_pos(buf));
    editor_destroy(ed);
}

static void test_editor_line_start_at_line_start_returns_false(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AAA", 3);
    const buffer_t *buf = editor_test_get_buffer(ed);
    buffer_set_cursor((buffer_t *)buf, 0);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_START));
    editor_destroy(ed);
}

static void test_editor_line_end_at_line_end_returns_false(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AAA", 3);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    editor_destroy(ed);
}

static void test_editor_line_start_empty_buffer_returns_false(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_START));
    TEST_ASSERT_FALSE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    editor_destroy(ed);
}

static void test_editor_line_end_multibyte_codepoint(void) {
    // Euro is 3 bytes. Layout treats the font as monospace and uses glyphs[0]
    // advance (10) for every codepoint. So 6 euros fit per line (x=80); the 7th
    // triggers wrap. Line 0 = [0, 18). Soft-wrap back-off must step back one
    // codepoint = 3 bytes, not 1 byte, to land at 15.
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed,
                       "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC"
                       "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC",
                       21);
    const buffer_t *buf = editor_test_get_buffer(ed);
    buffer_set_cursor((buffer_t *)buf, 0);
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LINE_END));
    TEST_ASSERT_EQUAL_size_t(15, buffer_cursor_pos(buf)); // 18 - 3
    editor_destroy(ed);
}

static void test_editor_render_clears_dirty_flag(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_NOT_NULL(ed);
    editor_insert_utf8(ed, "A", 1);
    TEST_ASSERT_TRUE(editor_is_dirty(ed));
    editor_render(ed);
    TEST_ASSERT_FALSE(editor_is_dirty(ed));
    editor_destroy(ed);
}

static void test_editor_initial_render_is_full(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_TRUE(editor_is_dirty(ed)); // first frame paints everything
    damage_t d = editor_render(ed);
    TEST_ASSERT_EQUAL(FLUSH_FULL, d.kind);
    TEST_ASSERT_FALSE(rect_is_empty(d.rect));
    TEST_ASSERT_FALSE(editor_is_dirty(ed));
    editor_destroy(ed);
}

static void test_editor_inline_edit_is_partial(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_render(ed); // clear the initial full damage
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "A", 1));
    TEST_ASSERT_TRUE(editor_is_dirty(ed));
    damage_t d = editor_render(ed);
    TEST_ASSERT_EQUAL(FLUSH_PARTIAL, d.kind);
    TEST_ASSERT_FALSE(rect_is_empty(d.rect));
    editor_destroy(ed);
}

static void test_editor_cursor_move_damages_partial(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AB", 2);
    editor_render(ed); // clear; records the cursor cell
    TEST_ASSERT_TRUE(editor_move_cursor(ed, EDITOR_CURSOR_LEFT));
    TEST_ASSERT_TRUE(editor_is_dirty(ed)); // old cursor cell must be repainted
    damage_t d = editor_render(ed);
    TEST_ASSERT_EQUAL(FLUSH_PARTIAL, d.kind);
    TEST_ASSERT_FALSE(rect_is_empty(d.rect));
    editor_destroy(ed);
}

static void test_editor_wrapping_edit_damages_whole_screen_partial(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_render(ed); // clear
    // 10 glyphs at advance 10 overrun the wrap width, so layout recomputes
    // and repaints the whole screen as a partial flush (not a full flush).
    TEST_ASSERT_TRUE(editor_insert_utf8(ed, "AAAAAAAAAA", 10));
    damage_t d = editor_render(ed);
    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    TEST_ASSERT_EQUAL(FLUSH_PARTIAL, d.kind);
    TEST_ASSERT_EQUAL_INT(0, d.rect.x);
    TEST_ASSERT_EQUAL_INT(0, d.rect.y);
    TEST_ASSERT_EQUAL_INT(fb->width, d.rect.w);
    TEST_ASSERT_EQUAL_INT(fb->height, d.rect.h);
    editor_destroy(ed);
}

// regression: typing must erase the previous underline. it sits below the
// text row, so an edit's row damage alone does not cover it.
static void test_typing_erases_old_underline(void) {
    editor_t *ed = editor_create(&font_ascent, 256);
    editor_render(ed); // initial frame: cursor at column 0

    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    int y = MARGIN_TOP + CURSOR_Y_OFFSET;
    int col0_x = MARGIN_X + 0 * 10; // advance 10
    int col1_x = MARGIN_X + 1 * 10;
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FOREGROUND, fb->pixels[y * fb->stride + col0_x],
                                    "cursor not drawn at column 0");

    editor_insert_utf8(ed, "A", 1); // cursor advances to column 1
    editor_render(ed);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BACKGROUND, fb->pixels[y * fb->stride + col0_x],
                                    "old underline left behind after typing");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FOREGROUND, fb->pixels[y * fb->stride + col1_x],
                                    "cursor not redrawn at column 1");
    editor_destroy(ed);
}

static void test_editor_contiguous_inserts_coalesce(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1);
    editor_insert_utf8(ed, "B", 1);
    editor_insert_utf8(ed, "A", 1);
    // contiguous typing folds into a single undo group
    TEST_ASSERT_EQUAL_size_t(1, editor_test_undo_count(ed));
    TEST_ASSERT_EQUAL_size_t(0, editor_test_redo_count(ed));
    // one undo clears the whole run
    TEST_ASSERT_TRUE(editor_undo(ed));
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(editor_test_get_buffer(ed)));
    editor_destroy(ed);
}

static void test_editor_delete_records_on_undo_stack(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1); // 1 command
    editor_backspace(ed);           // 2 commands (insert + delete)
    TEST_ASSERT_EQUAL_size_t(2, editor_test_undo_count(ed));
    editor_destroy(ed);
}

static void test_editor_failed_insert_does_not_record(void) {
    editor_t *ed = editor_create(&font, 1);
    TEST_ASSERT_FALSE(editor_insert_utf8(ed, "AB", 2)); // overflows capacity 1
    TEST_ASSERT_EQUAL_size_t(0, editor_test_undo_count(ed));
    editor_destroy(ed);
}

static void test_editor_at_edge_noop_does_not_record(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_FALSE(editor_backspace(ed));      // cursor at 0
    TEST_ASSERT_FALSE(editor_delete_forward(ed)); // cursor at end
    TEST_ASSERT_EQUAL_size_t(0, editor_test_undo_count(ed));
    editor_destroy(ed);
}

// exercises ring wrap, eviction, and freeing the dropped payload (ASan watches
// for leaks / use-after-free here).
static void test_editor_undo_ring_caps_and_drops_oldest(void) {
    editor_t *ed = editor_create(&font, 1024);
    // newline inserts never coalesce, so each is its own undo group
    for (int i = 0; i < 300; i++) { // > UNDO_CAP (256)
        TEST_ASSERT_TRUE(editor_insert_utf8(ed, "\n", 1));
    }
    TEST_ASSERT_EQUAL_size_t(256, editor_test_undo_count(ed));
    editor_destroy(ed);
}

static void test_editor_undo_empty_returns_false(void) {
    editor_t *ed = editor_create(&font, 256);
    TEST_ASSERT_FALSE(editor_undo(ed));
    TEST_ASSERT_FALSE(editor_redo(ed));
    editor_destroy(ed);
}

static void test_editor_undo_insert_reverts_buffer_and_cursor(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AB", 2);
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(2, buffer_size(buf));

    TEST_ASSERT_TRUE(editor_undo(ed));
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));       // insert undone
    TEST_ASSERT_EQUAL_size_t(0, buffer_cursor_pos(buf)); // cursor back to pos
    TEST_ASSERT_EQUAL_size_t(0, editor_test_undo_count(ed));
    TEST_ASSERT_EQUAL_size_t(1, editor_test_redo_count(ed));
    editor_destroy(ed);
}

static void test_editor_redo_replays_insert(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "AB", 2);
    editor_undo(ed);
    TEST_ASSERT_TRUE(editor_redo(ed));
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(2, buffer_size(buf));
    TEST_ASSERT_EQUAL('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL('B', buffer_char_at(buf, 1));
    TEST_ASSERT_EQUAL_size_t(2, buffer_cursor_pos(buf)); // cursor after inserted text
    TEST_ASSERT_EQUAL_size_t(1, editor_test_undo_count(ed));
    TEST_ASSERT_EQUAL_size_t(0, editor_test_redo_count(ed));
    editor_destroy(ed);
}

static void test_editor_undo_delete_restores_bytes(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "\xE2\x82\xAC", 3); // Euro, then backspace it away
    editor_backspace(ed);
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(0, buffer_size(buf));

    TEST_ASSERT_TRUE(editor_undo(ed)); // undo the backspace → Euro restored
    TEST_ASSERT_EQUAL_size_t(3, buffer_size(buf));
    TEST_ASSERT_EQUAL((char)0xE2, buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL((char)0x82, buffer_char_at(buf, 1));
    TEST_ASSERT_EQUAL((char)0xAC, buffer_char_at(buf, 2));
    editor_destroy(ed);
}

static void test_editor_undo_redo_multistep(void) {
    editor_t *ed = editor_create(&font, 256);
    // newlines keep these as three distinct undo groups: "A", "\n", "B"
    editor_insert_utf8(ed, "A", 1);
    editor_insert_utf8(ed, "\n", 1);
    editor_insert_utf8(ed, "B", 1); // "A\nB"
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(3, editor_test_undo_count(ed));

    editor_undo(ed); // "A\n"
    editor_undo(ed); // "A"
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf));
    TEST_ASSERT_EQUAL_size_t(1, editor_test_undo_count(ed));
    TEST_ASSERT_EQUAL_size_t(2, editor_test_redo_count(ed));

    editor_redo(ed); // "A\n"
    TEST_ASSERT_EQUAL_size_t(2, buffer_size(buf));
    TEST_ASSERT_EQUAL('\n', buffer_char_at(buf, 1));
    editor_destroy(ed);
}

// a fresh edit after undo discards the redo future (and frees its payloads —
// ASan watches here).
static void test_editor_new_edit_clears_redo(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1);
    editor_insert_utf8(ed, "\n", 1); // distinct group
    editor_undo(ed);                 // redo now holds the "\n" insert
    TEST_ASSERT_EQUAL_size_t(1, editor_test_redo_count(ed));

    editor_insert_utf8(ed, "B", 1); // new edit → redo discarded
    TEST_ASSERT_EQUAL_size_t(0, editor_test_redo_count(ed));
    TEST_ASSERT_FALSE(editor_redo(ed)); // nothing to redo
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL('B', buffer_char_at(buf, 1)); // the new edit, not "\n"
    editor_destroy(ed);
}

static void test_editor_newline_breaks_insert_coalesce(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "a", 1);
    editor_insert_utf8(ed, "\n", 1); // own group; can't fold either side
    editor_insert_utf8(ed, "b", 1);
    TEST_ASSERT_EQUAL_size_t(3, editor_test_undo_count(ed));
    editor_destroy(ed);
}

static void test_editor_cursor_move_breaks_coalesce(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "A", 1);
    editor_insert_utf8(ed, "B", 1); // coalesces → "AB", one group
    TEST_ASSERT_EQUAL_size_t(1, editor_test_undo_count(ed));
    // move away and back to the same spot: geometry would allow folding, but the
    // cursor jump must break the chain
    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
    editor_move_cursor(ed, EDITOR_CURSOR_RIGHT);
    editor_insert_utf8(ed, "C", 1);
    TEST_ASSERT_EQUAL_size_t(2, editor_test_undo_count(ed));
    editor_destroy(ed);
}

static void test_editor_backspaces_coalesce_and_restore(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "abc", 3); // one insert group
    editor_backspace(ed);             // delete 'c' (new group: differs from insert)
    editor_backspace(ed);             // delete 'b', folds left into the delete group
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf)); // "a"
    TEST_ASSERT_EQUAL_size_t(2, editor_test_undo_count(ed));

    TEST_ASSERT_TRUE(editor_undo(ed)); // one undo restores both "bc"
    TEST_ASSERT_EQUAL_size_t(3, buffer_size(buf));
    TEST_ASSERT_EQUAL('a', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL('b', buffer_char_at(buf, 1));
    TEST_ASSERT_EQUAL('c', buffer_char_at(buf, 2));
    editor_destroy(ed);
}

static void test_editor_delete_forward_coalesces(void) {
    editor_t *ed = editor_create(&font, 256);
    editor_insert_utf8(ed, "abc", 3);
    buffer_set_cursor((buffer_t *)editor_test_get_buffer(ed), 0);
    editor_delete_forward(ed); // delete 'a' (new group)
    editor_delete_forward(ed); // delete 'b', folds right into the delete group
    const buffer_t *buf = editor_test_get_buffer(ed);
    TEST_ASSERT_EQUAL_size_t(1, buffer_size(buf)); // "c"
    TEST_ASSERT_EQUAL_size_t(2, editor_test_undo_count(ed));

    TEST_ASSERT_TRUE(editor_undo(ed)); // restores "ab" ahead of "c"
    TEST_ASSERT_EQUAL_size_t(3, buffer_size(buf));
    TEST_ASSERT_EQUAL('a', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL('b', buffer_char_at(buf, 1));
    TEST_ASSERT_EQUAL('c', buffer_char_at(buf, 2));
    editor_destroy(ed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_editor_undo_empty_returns_false);
    RUN_TEST(test_editor_undo_insert_reverts_buffer_and_cursor);
    RUN_TEST(test_editor_redo_replays_insert);
    RUN_TEST(test_editor_undo_delete_restores_bytes);
    RUN_TEST(test_editor_undo_redo_multistep);
    RUN_TEST(test_editor_new_edit_clears_redo);
    RUN_TEST(test_editor_newline_breaks_insert_coalesce);
    RUN_TEST(test_editor_cursor_move_breaks_coalesce);
    RUN_TEST(test_editor_backspaces_coalesce_and_restore);
    RUN_TEST(test_editor_delete_forward_coalesces);
    RUN_TEST(test_editor_contiguous_inserts_coalesce);
    RUN_TEST(test_editor_delete_records_on_undo_stack);
    RUN_TEST(test_editor_failed_insert_does_not_record);
    RUN_TEST(test_editor_at_edge_noop_does_not_record);
    RUN_TEST(test_editor_undo_ring_caps_and_drops_oldest);
    RUN_TEST(test_editor_create_returns_non_null_for_valid_args);
    RUN_TEST(test_editor_render_clears_dirty_flag);
    RUN_TEST(test_typing_erases_old_underline);
    RUN_TEST(test_editor_initial_render_is_full);
    RUN_TEST(test_editor_inline_edit_is_partial);
    RUN_TEST(test_editor_cursor_move_damages_partial);
    RUN_TEST(test_editor_wrapping_edit_damages_whole_screen_partial);
    RUN_TEST(test_editor_create_returns_null_for_null_font);
    RUN_TEST(test_editor_create_returns_null_for_zero_capacity);
    RUN_TEST(test_editor_create_returns_null_for_font_with_no_glyphs);
    RUN_TEST(test_editor_destroy_null_safe);
    RUN_TEST(test_editor_insert_utf8_updates_buffer);
    RUN_TEST(test_editor_insert_utf8_sets_dirty);
    RUN_TEST(test_editor_insert_utf8_overflow_returns_false);
    RUN_TEST(test_editor_backspace_at_zero_returns_false);
    RUN_TEST(test_editor_backspace_deletes_one_codepoint_ascii);
    RUN_TEST(test_editor_backspace_deletes_one_codepoint_multibyte_euro);
    RUN_TEST(test_editor_delete_forward_at_end_returns_false);
    RUN_TEST(test_editor_delete_forward_deletes_one_codepoint_multibyte);
    RUN_TEST(test_editor_move_cursor_left_at_zero_returns_false);
    RUN_TEST(test_editor_move_cursor_right_at_end_returns_false);
    RUN_TEST(test_editor_move_cursor_left_advances_by_codepoint_multibyte);
    RUN_TEST(test_editor_move_cursor_right_advances_by_codepoint_multibyte);
    RUN_TEST(test_editor_line_start_jumps_to_start_of_visual_line);
    RUN_TEST(test_editor_line_end_lands_before_newline);
    RUN_TEST(test_editor_line_end_backs_off_one_codepoint_on_soft_wrap);
    RUN_TEST(test_editor_line_end_lands_at_buffer_end_on_final_line);
    RUN_TEST(test_editor_line_start_at_line_start_returns_false);
    RUN_TEST(test_editor_line_end_at_line_end_returns_false);
    RUN_TEST(test_editor_line_start_empty_buffer_returns_false);
    RUN_TEST(test_editor_line_end_multibyte_codepoint);
    return UNITY_END();
}
