#include "hal/hal_display.h"

#include <SDL2/SDL.h>
#include <SDL_pixels.h>
#include <SDL_render.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540

// SDL-only state, file-scope statics
static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture *g_texture;
static hal_framebuffer_t g_framebuffer;
static uint8_t *g_pixel_buffer;
static uint8_t *g_rgb_buffer;

static void copy_grayscale_to_rgb24(const uint8_t *gray, uint8_t *rgb, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; i++) {
        rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = gray[i];
    }
}

int hal_display_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    g_window = SDL_CreateWindow("Foolscap", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto fail_after_init;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto fail_after_window;
    }

    g_pixel_buffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT);
    if (!g_pixel_buffer) {
        fprintf(stderr, "Error: could not malloc g_pixel_buffer\n");
        goto fail_after_renderer;
    }

    g_rgb_buffer = malloc((size_t)SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    if (!g_rgb_buffer) {
        fprintf(stderr, "Error: could not malloc g_rgb_buffer\n");
        goto fail_after_pixel_buffer;
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                  SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        goto fail_after_rgb_buffer;
    }

    g_framebuffer.pixels = g_pixel_buffer;
    g_framebuffer.width = SCREEN_WIDTH;
    g_framebuffer.height = SCREEN_HEIGHT;
    g_framebuffer.stride = SCREEN_WIDTH;
    SDL_StartTextInput();
    return 0;

fail_after_rgb_buffer:
    free(g_rgb_buffer);
fail_after_pixel_buffer:
    free(g_pixel_buffer);
fail_after_renderer:
    SDL_DestroyRenderer(g_renderer);
fail_after_window:
    SDL_DestroyWindow(g_window);
fail_after_init:
    SDL_Quit();
    return -1;
}

void hal_display_shutdown(void) {
    SDL_StopTextInput();
    SDL_DestroyTexture(g_texture);
    free(g_rgb_buffer);
    free(g_pixel_buffer);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}

hal_framebuffer_t *hal_display_get_framebuffer(void) {
    return &g_framebuffer;
}

void hal_display_clear(uint8_t value) {
    memset(g_pixel_buffer, value, SCREEN_WIDTH * SCREEN_HEIGHT);
}

int hal_display_flush(void) {
    copy_grayscale_to_rgb24(g_pixel_buffer, g_rgb_buffer, SCREEN_WIDTH * SCREEN_HEIGHT);
    if (SDL_UpdateTexture(g_texture, NULL, g_rgb_buffer, SCREEN_WIDTH * 3) != 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
    return 0;
}
