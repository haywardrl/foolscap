#include "app/editor/editor.h"
#include "app/render/fonts/ibm_plex_mono_32.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "hal/hal_display.h"
#include "input_esp32s3.h"
#include "key_event.h"
#include "nvs_flash.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#define DOCUMENT_CAPACITY (256 * 1024)
#define KEY_QUEUE_DEPTH 256

// anti-ghost full refresh fires on typing pauses, not a keystroke count.
// forward typing barely ghosts, so deferring the refresh keeps it out of
// the active typing flow. a high backstop covers sustained typing with no
// pause.
#define IDLE_FULL_REFRESH_MS 1500
#define PARTIALS_BACKSTOP 200

static const char *TAG = "foolscap";

static void apply_key(editor_t *ed, const key_event_t *k) {
    switch (k->kind) {
    case KEY_INSERT:
        editor_insert_utf8(ed, k->bytes, k->len);
        break;
    case KEY_BACKSPACE:
        editor_backspace(ed);
        break;
    case KEY_DELETE:
        editor_delete_forward(ed);
        break;
    case KEY_LEFT:
        editor_move_cursor(ed, EDITOR_CURSOR_LEFT);
        break;
    case KEY_RIGHT:
        editor_move_cursor(ed, EDITOR_CURSOR_RIGHT);
        break;
    case KEY_UP:
        editor_move_cursor(ed, EDITOR_CURSOR_UP);
        break;
    case KEY_DOWN:
        editor_move_cursor(ed, EDITOR_CURSOR_DOWN);
        break;
    case KEY_WORD_LEFT:
        editor_move_cursor(ed, EDITOR_CURSOR_WORD_LEFT);
        break;
    case KEY_WORD_RIGHT:
        editor_move_cursor(ed, EDITOR_CURSOR_WORD_RIGHT);
        break;
    case KEY_LINE_START:
        editor_move_cursor(ed, EDITOR_CURSOR_LINE_START);
        break;
    case KEY_LINE_END:
        editor_move_cursor(ed, EDITOR_CURSOR_LINE_END);
        break;
    }
}

// render pending damage and push to the panel. returns true if it did a
// partial (region) flush, meaning an anti-ghost full refresh is owed once
// typing pauses. false if it already did a full flush.
static bool present(editor_t *ed) {
    int64_t t0 = esp_timer_get_time();
    damage_t d = editor_render(ed);
    ESP_LOGI(TAG, "render damage kind=%d rect=%d,%d %dx%d", d.kind, d.rect.x, d.rect.y, d.rect.w,
             d.rect.h);
    bool did_partial;
    if (d.kind == FLUSH_FULL) {
        hal_display_full_flush();
        did_partial = false;
    } else {
        hal_display_flush_region(d.rect.x, d.rect.y, d.rect.w, d.rect.h);
        did_partial = true;
    }
    ESP_LOGI(TAG, "present done in %lld ms", (long long)((esp_timer_get_time() - t0) / 1000));
    return did_partial;
}

void app_main(void) {
    ESP_LOGI(TAG, "boot");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (hal_display_init() != 0) {
        ESP_LOGE(TAG, "hal_display_init failed");
        return;
    }

    editor_t *ed = editor_create(&IBM_PLEX_MONO_32, DOCUMENT_CAPACITY);
    if (ed == NULL) {
        ESP_LOGE(TAG, "editor_create failed");
        return;
    }

    present(ed); // clear the panel, show the empty document + cursor

    QueueHandle_t keys = xQueueCreate(KEY_QUEUE_DEPTH, sizeof(key_event_t));
    if (keys == NULL) {
        ESP_LOGE(TAG, "key queue alloc failed");
        return;
    }

    input_start(keys); // NVS is ready, so NimBLE bond storage works

    // Coalesce whatever arrived while rendering, present partials as we type,
    // and do the anti-ghost full refresh when typing pauses (or as a backstop).
    key_event_t k;
    int pending_partials = 0; // partial flushes since the last full refresh
    TickType_t wait = portMAX_DELAY;
    while (1) {
        if (xQueueReceive(keys, &k, wait) == pdTRUE) {
            apply_key(ed, &k);
            while (xQueueReceive(keys, &k, 0) == pdTRUE) {
                apply_key(ed, &k);
            }
            if (editor_is_dirty(ed)) {
                if (present(ed)) {
                    if (++pending_partials >= PARTIALS_BACKSTOP) {
                        hal_display_full_flush();
                        pending_partials = 0;
                    }
                } else {
                    pending_partials = 0; // full flush already cleaned the panel
                }
            }
            // short wait for next keystroke. a lull triggers cleanup.
            wait = pending_partials > 0 ? pdMS_TO_TICKS(IDLE_FULL_REFRESH_MS) : portMAX_DELAY;
        } else if (pending_partials > 0) {
            // typing paused with partial updates outstanding: clean up once
            ESP_LOGI(TAG, "idle full refresh after %d partials", pending_partials);
            hal_display_full_flush();
            pending_partials = 0;
            wait = portMAX_DELAY;
        }
    }
}
