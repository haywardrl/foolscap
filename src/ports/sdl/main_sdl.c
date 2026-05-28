#include "app/editor/editor.h"
#include "app/render/fonts/ibm_plex_mono_32.h"
#include "hal/hal_display.h"

#include <SDL2/SDL.h>

#define DOCUMENT_CAPACITY (256 * 1024) // 256 KB

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int rc = 1;
    editor_t *ed = NULL;

    if (hal_display_init() != 0)
        return 1;
    SDL_StartTextInput();

    ed = editor_create(&IBM_PLEX_MONO_32, DOCUMENT_CAPACITY);
    if (ed == NULL)
        goto cleanup;

    // Initial render
    editor_render(ed);
    hal_display_full_flush();

    // Event loop
    int running = 1;
    while (running) {
        SDL_WaitEvent(NULL);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_TEXTINPUT:
                // suppress text inserts when Alt is held so emacs-style
                // M-f / M-b don't also type 'f' / 'b'.
                if (SDL_GetModState() & KMOD_ALT) {
                    break;
                }
                // event.text.text is null-terminated UTF-8
                editor_insert_utf8(ed, event.text.text, strlen(event.text.text));
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    editor_insert_utf8(ed, "\n", 1);
                    break;
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                case SDLK_BACKSPACE:
                    editor_backspace(ed);
                    break;
                case SDLK_DELETE:
                    editor_delete_forward(ed);
                    break;
                case SDLK_LEFT: {
                    bool word = (event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT)) != 0;
                    editor_move_cursor(ed, word ? EDITOR_CURSOR_WORD_LEFT : EDITOR_CURSOR_LEFT);
                    break;
                }
                case SDLK_RIGHT: {
                    bool word = (event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT)) != 0;
                    editor_move_cursor(ed, word ? EDITOR_CURSOR_WORD_RIGHT : EDITOR_CURSOR_RIGHT);
                    break;
                }
                case SDLK_b:
                    if (event.key.keysym.mod & KMOD_ALT) {
                        editor_move_cursor(ed, EDITOR_CURSOR_WORD_LEFT);
                    } else if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
                    }
                    break;
                case SDLK_f:
                    if (event.key.keysym.mod & KMOD_ALT) {
                        editor_move_cursor(ed, EDITOR_CURSOR_WORD_RIGHT);
                    } else if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_RIGHT);
                    }
                    break;
                case SDLK_a:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_LINE_START);
                    }
                    break;
                case SDLK_e:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_LINE_END);
                    }
                    break;
                case SDLK_UP:
                    editor_move_cursor(ed, EDITOR_CURSOR_UP);
                    break;
                case SDLK_DOWN:
                    editor_move_cursor(ed, EDITOR_CURSOR_DOWN);
                    break;
                case SDLK_n:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_DOWN);
                    }
                    break;
                case SDLK_p:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        editor_move_cursor(ed, EDITOR_CURSOR_UP);
                    }
                    break;
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    hal_display_full_flush();
                }
                break;
            }
        }

        if (editor_is_dirty(ed)) {
            damage_t d = editor_render(ed);
            if (d.kind == FLUSH_FULL) {
                hal_display_full_flush();
            } else {
                hal_display_flush_region(d.rect.x, d.rect.y, d.rect.w, d.rect.h);
            }
        }
    }

    rc = 0;
cleanup:
    if (ed)
        editor_destroy(ed);
    SDL_StopTextInput();
    hal_display_shutdown();
    return rc;
}
