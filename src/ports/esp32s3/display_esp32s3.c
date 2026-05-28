#include "epd_driver.h"
#include "hal/hal_display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *TAG = "display";

// 1-bit fast-update tunables. each pass is one waveform frame over the band.
// lighten drives toward white (erase old ink), darken drives toward black.
// residual is cleaned by the periodic full refresh.
#define REGION_LIGHTEN_PASSES 4
#define REGION_DARKEN_PASSES 6
#define REGION_FRAME_TIME 30 // per-row output time (driver "dus" units)

// full refresh: black/white clear flash (deghost), then 1-bit darken of all
// text. each clear cycle is 8 full-screen waveform passes, so cycle count is
// the main speed lever. differential partials keep ghosting low, so a light
// 2-cycle deghost is usually enough. raise if ghosting builds, lower for speed.
#define FULL_CLEAR_CYCLES 2
#define FULL_CLEAR_CYCLE_TIME 50
#define FULL_DARKEN_PASSES 8

#define STRIDE_1BPP (EPD_WIDTH / 8)

// native landscape, framebuffer matches the panel 1:1 (960x540). app draws
// 8bpp grayscale (0=black, 255=white), flush thresholds to 1-bit and drives
// the panel via the LilyGo EPD47 1-bit waveform. content is pure b/w text,
// so 1-bit is all we need and it's far faster than the grayscale path.
//
// driver: vendored LilyGo-EPD47 (components/lilygo_epd47), manufacturer lib
// for the non-pro T5 4.7" S3 (ED047TC1 panel, 74HC shift register + esp_lcd
// i80 parallel bus). no high-level diff engine, we own the 8bpp framebuffer
// and pack regions ourselves.
static hal_framebuffer_t g_fb;
static uint8_t *g_pixels;     // app-facing 8bpp framebuffer (PSRAM)
static uint8_t *g_darken;     // 1bpp diff mask: pixels to drive black, stride EPD_WIDTH/8
static uint8_t *g_lighten;    // 1bpp diff mask: pixels to drive white, stride EPD_WIDTH/8
static uint8_t *g_panel;      // 1bpp model of what is physically on the panel now

typedef struct {
    bool any_darken;
    bool any_lighten;
} diff_t;

// diff rows [y, y+h) of the 8bpp framebuffer against panel state (g_panel)
// into two 1bpp masks: g_darken = pixels that became ink, g_lighten = pixels
// that became background. update g_panel to the new state. driving only
// changed pixels avoids re-pulsing static text (which bloomed to grey).
// bit order within each byte is LSB = leftmost pixel (x & 7), matching
// calc_epd_input_1bpp.
static diff_t build_diff_masks(int y, int h) {
    diff_t d = {false, false};
    for (int row = 0; row < h; row++) {
        const uint8_t *src = g_pixels + (size_t)(y + row) * g_fb.stride;
        size_t off = (size_t)(y + row) * STRIDE_1BPP;
        uint8_t *dk = g_darken + off;
        uint8_t *lt = g_lighten + off;
        uint8_t *pv = g_panel + off;
        memset(dk, 0, STRIDE_1BPP);
        memset(lt, 0, STRIDE_1BPP);
        for (int x = 0; x < EPD_WIDTH; x++) {
            uint8_t bit = (uint8_t)(1u << (x & 7));
            bool now_black = src[x] < 128;
            bool was_black = (pv[x >> 3] & bit) != 0;
            if (now_black && !was_black) {
                dk[x >> 3] |= bit;
                pv[x >> 3] |= bit;
                d.any_darken = true;
            } else if (!now_black && was_black) {
                lt[x >> 3] |= bit;
                pv[x >> 3] &= (uint8_t)~bit;
                d.any_lighten = true;
            }
        }
    }
    return d;
}

int hal_display_init(void) {
    epd_init();

    size_t count = (size_t)EPD_WIDTH * (size_t)EPD_HEIGHT;
    size_t mask_bytes = (size_t)STRIDE_1BPP * EPD_HEIGHT;
    g_pixels = heap_caps_malloc(count, MALLOC_CAP_SPIRAM);
    g_darken = heap_caps_malloc(mask_bytes, MALLOC_CAP_SPIRAM);
    g_lighten = heap_caps_malloc(mask_bytes, MALLOC_CAP_SPIRAM);
    g_panel = heap_caps_malloc(mask_bytes, MALLOC_CAP_SPIRAM);
    if (g_pixels == NULL || g_darken == NULL || g_lighten == NULL || g_panel == NULL) {
        ESP_LOGE(TAG, "failed to allocate framebuffers");
        return -1;
    }
    memset(g_pixels, 0xFF, count);
    memset(g_panel, 0, mask_bytes); // panel cleared white below = no ink

    g_fb.pixels = g_pixels;
    g_fb.width = EPD_WIDTH;
    g_fb.height = EPD_HEIGHT;
    g_fb.stride = EPD_WIDTH;

    // clean white baseline before the first draw (epd_draw_image assumes white)
    epd_poweron();
    epd_clear();
    epd_poweroff();
    ESP_LOGI(TAG, "lilygo_epd47 display up: %dx%d", EPD_WIDTH, EPD_HEIGHT);
    return 0;
}

void hal_display_shutdown(void) {
    if (g_pixels) {
        heap_caps_free(g_pixels);
        g_pixels = NULL;
    }
    if (g_darken) {
        heap_caps_free(g_darken);
        g_darken = NULL;
    }
    if (g_lighten) {
        heap_caps_free(g_lighten);
        g_lighten = NULL;
    }
    if (g_panel) {
        heap_caps_free(g_panel);
        g_panel = NULL;
    }
}

hal_framebuffer_t *hal_display_get_framebuffer(void) {
    return &g_fb;
}

void hal_display_clear(uint8_t value) {
    memset(g_pixels, value, (size_t)g_fb.width * (size_t)g_fb.height);
}

int hal_display_full_flush(void) {
    int64_t t0 = esp_timer_get_time();
    Rect_t area = epd_full_screen();
    // the clear flash drives the whole panel white, so reset the panel model
    // to "no ink" and let the diff make the darken mask cover all text.
    memset(g_panel, 0, (size_t)STRIDE_1BPP * EPD_HEIGHT);
    build_diff_masks(0, EPD_HEIGHT);
    epd_poweron();
    // deghost flash: a few clear cycles. epd_clear_area_cycles is a busy-spin
    // on CPU0 that never yields, so run one cycle at a time and vTaskDelay
    // between them so IDLE0 gets scheduled and resets its task WDT.
    for (int c = 0; c < FULL_CLEAR_CYCLES; c++) {
        epd_clear_area_cycles(area, 1, FULL_CLEAR_CYCLE_TIME);
        vTaskDelay(1);
    }
    int64_t t_cleared = esp_timer_get_time();
    // draw text with the fast 1-bit darken. clear left the panel white, so
    // only the darken mask (= all text) applies.
    for (int p = 0; p < FULL_DARKEN_PASSES; p++) {
        epd_draw_frame_1bit(area, g_darken, BLACK_ON_WHITE, REGION_FRAME_TIME);
    }
    epd_poweroff();
    int64_t t_end = esp_timer_get_time();
    ESP_LOGI(TAG, "full flush in %lld ms (clear %lld, draw %lld)",
             (long long)((t_end - t0) / 1000), (long long)((t_cleared - t0) / 1000),
             (long long)((t_end - t_cleared) / 1000));
    return 0;
}

int hal_display_flush_region(int x, int y, int w, int h) {
    // always refresh the full-width band spanning the damaged rows, not the
    // tight rect. the editor's damage for an edit is already a full-width
    // line strip, and a full-width band (area.width == EPD_WIDTH) lets us
    // pass a slice of the full-screen 1bpp masks directly (stride matches),
    // avoiding the driver's odd-width / nibble-shift sub-buffer path.
    (void)x;
    (void)w;
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (y + h > EPD_HEIGHT)
        h = EPD_HEIGHT - y;
    if (h <= 0)
        return 0;

    int64_t t0 = esp_timer_get_time();
    diff_t d = build_diff_masks(y, h);
    if (!d.any_darken && !d.any_lighten) {
        return 0; // panel already matches
    }

    // differential 1-bit update: drive only the pixels that changed vs the
    // panel model. lighten (erase) removed ink, darken newly added ink.
    // static text gets no pulse, so it can't bloom to grey. skip a direction
    // when nothing changed that way (forward typing needs no lighten).
    Rect_t area = {.x = 0, .y = y, .width = EPD_WIDTH, .height = h};
    uint8_t *darken = g_darken + (size_t)y * STRIDE_1BPP;
    uint8_t *lighten = g_lighten + (size_t)y * STRIDE_1BPP;
    epd_poweron();
    if (d.any_lighten) {
        for (int p = 0; p < REGION_LIGHTEN_PASSES; p++) {
            epd_draw_frame_1bit(area, lighten, WHITE_ON_BLACK, REGION_FRAME_TIME);
        }
    }
    if (d.any_darken) {
        for (int p = 0; p < REGION_DARKEN_PASSES; p++) {
            epd_draw_frame_1bit(area, darken, BLACK_ON_WHITE, REGION_FRAME_TIME);
        }
    }
    epd_poweroff();
    ESP_LOGI(TAG, "region flush rows %d..%d (dk=%d lt=%d) in %lld ms", y, y + h, d.any_darken,
             d.any_lighten, (long long)((esp_timer_get_time() - t0) / 1000));
    return 0;
}
