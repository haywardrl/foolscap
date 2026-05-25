#include "app/editor/editor.h"
#include "app/render/fonts/ibm_plex_mono_20.h"
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

    ed = editor_create(&IBM_PLEX_MONO_20, DOCUMENT_CAPACITY);
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
                case SDLK_LEFT:
                    editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
                    break;
                case SDLK_RIGHT:
                    editor_move_cursor(ed, EDITOR_CURSOR_RIGHT);
                    break;
                case SDLK_UP:
                    editor_move_cursor(ed, EDITOR_CURSOR_UP);
                    break;
                case SDLK_DOWN:
                    editor_move_cursor(ed, EDITOR_CURSOR_DOWN);
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
            editor_render(ed);
            // TODO: replace with editor_dirty_rect(ed) once the editor
            // tracks the changed region for partial e-ink refresh.
            hal_framebuffer_t *fb = hal_display_get_framebuffer();
            hal_display_flush_region(0, 0, fb->width, fb->height);
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
