#include "app/render/fonts/ibm_plex_mono_20.h"
#include "hal/hal_display.h"
#include "render.h"

#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int rc = 1;
    if (hal_display_init() != 0) {
        return 1;
    }
    hal_display_clear(255);
    hal_framebuffer_t *fb = hal_display_get_framebuffer();
    if (fb == NULL) {
        goto cleanup;
    }
    static const char msg[] = "Hello, Foolscap!";
    render_draw_string(fb, &IBM_PLEX_MONO_20, 10, 30, msg, sizeof(msg) - 1);
    hal_display_flush();

    SDL_Event event;
    int running = 1;
    while (running) {
        SDL_WaitEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            running = 0;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                hal_display_flush();
            }
            break;
        }
    }
    rc = 0;
cleanup:
    hal_display_shutdown();
    return rc;
}
