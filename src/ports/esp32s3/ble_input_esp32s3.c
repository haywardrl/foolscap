#include "esp_hid_gap.h"
#include "hid_keymap.h"
#include "input_esp32s3.h"

#include "esp_gattc_api.h"
#include "esp_hidh.h"

#include <esp_log.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

#define SCAN_DURATION_SECONDS 5
#define APPEARANCE_KEYBOARD 0x03C1

static const char *TAG = "ble_input";

static QueueHandle_t s_queue;
static uint8_t s_prev_keys[6];
static uint32_t s_drop_count;
static bool s_caps_lock;

static void scan_task(void *arg);

static bool was_down(uint8_t kc) {
    for (int i = 0; i < 6; i++) {
        if (s_prev_keys[i] == kc) {
            return true;
        }
    }
    return false;
}

static bool is_keyboard_usage(esp_hid_usage_t usage) {
    if (usage == 0) {
        return true;
    }
    return (usage & ESP_HID_USAGE_KEYBOARD) != 0;
}

static void on_report(esp_hid_usage_t usage, uint16_t report_id, const uint8_t *data,
                      uint16_t len) {
    if (!is_keyboard_usage(usage)) {
        return;
    }

    // boot keyboard report is 8 bytes: [mods][reserved][6 keycodes]. some
    // report-mode notifications prepend the report id as byte 0 even though
    // ESP-IDF also exposes it separately.
    uint16_t offset = 0;
    if (report_id != 0 && len >= 9 && data[0] == (uint8_t)report_id) {
        offset = 1;
    }

    if (len - offset < 8) {
        ESP_LOGW(TAG, "short keyboard report: usage=0x%x id=%u len=%u", usage, report_id, len);
        return;
    }

    uint8_t mods = data[offset];
    const uint8_t *keys = &data[offset + 2];

    for (int i = 0; i < 6; i++) {
        uint8_t kc = keys[i];
        if (kc == 0 || was_down(kc)) {
            continue;
        }
        if (kc == 0x39) { // caps lock: toggle host state, emit nothing
            s_caps_lock = !s_caps_lock;
            continue;
        }
        key_event_t ev;
        if (hid_decode(mods, kc, s_caps_lock, &ev)) {
            if (xQueueSend(s_queue, &ev, 0) != pdTRUE) {
                s_drop_count++;
                if (s_drop_count == 1 || (s_drop_count % 32) == 0) {
                    ESP_LOGW(TAG, "key queue full, dropped %lu keys", (unsigned long)s_drop_count);
                }
            }
        }
    }
    memcpy(s_prev_keys, keys, sizeof(s_prev_keys));
}

static void hidh_callback(void *args, esp_event_base_t base, int32_t id, void *event_data) {
    (void)args;
    (void)base;
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *p = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT:
        if (p->open.status == ESP_OK) {
            ESP_LOGI(TAG, "keyboard connected");
            esp_hidh_dev_dump(p->open.dev, stdout);
        } else {
            ESP_LOGE(TAG, "open failed");
        }
        break;
    case ESP_HIDH_INPUT_EVENT:
        ESP_LOGD(TAG, "input usage=0x%x map=%u id=%u len=%d", p->input.usage,
                 p->input.map_index, p->input.report_id, p->input.length);
        ESP_LOG_BUFFER_HEXDUMP(TAG, p->input.data, p->input.length, ESP_LOG_DEBUG);
        on_report(p->input.usage, p->input.report_id, p->input.data, p->input.length);
        break;
    case ESP_HIDH_CLOSE_EVENT:
        ESP_LOGW(TAG, "keyboard disconnected; rescanning");
        memset(s_prev_keys, 0, sizeof(s_prev_keys));
        xTaskCreate(scan_task, "ble_scan", 6 * 1024, NULL, 5, NULL);
        break;
    default:
        break;
    }
}

static void scan_task(void *arg) {
    (void)arg;
    while (1) {
        size_t count = 0;
        esp_hid_scan_result_t *results = NULL;
        ESP_LOGI(TAG, "scanning for a keyboard...");
        esp_hid_scan(SCAN_DURATION_SECONDS, &count, &results);
        ESP_LOGI(TAG, "scan found %u devices", (unsigned)count);

        // match on advertised appearance/usage. usage isn't populated until
        // the HID report map is read post-connect, so the keyboard appearance
        // (0x03C1) is what's actually available during a scan.
        esp_hid_scan_result_t *kbd = NULL;
        for (esp_hid_scan_result_t *r = results; r != NULL; r = r->next) {
            if (r->transport == ESP_HID_TRANSPORT_BLE &&
                (r->ble.appearance == APPEARANCE_KEYBOARD ||
                 (r->usage & ESP_HID_USAGE_KEYBOARD) != 0)) {
                kbd = r;
                break;
            }
        }

        if (kbd != NULL) {
            ESP_LOGI(TAG, "keyboard found: %s", kbd->name ? kbd->name : "");
            ESP_LOGI(TAG, "if prompted, type the passkey shown above on the keyboard");
            esp_hidh_dev_open(kbd->bda, kbd->transport, kbd->ble.addr_type);
            esp_hid_scan_results_free(results);
            break; // CLOSE event restarts scanning if it drops
        }

        esp_hid_scan_results_free(results);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void input_start(QueueHandle_t out) {
    s_queue = out;

    ESP_ERROR_CHECK(esp_hid_gap_init(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));

    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(esp_hidh_init(&config));

    xTaskCreate(scan_task, "ble_scan", 6 * 1024, NULL, 5, NULL);
}
