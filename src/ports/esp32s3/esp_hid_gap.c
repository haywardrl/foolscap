/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Trimmed from the ESP-IDF esp_hid_host example to the BLE-only Bluedroid
 * paths that foolscap actually uses.
 */

#include "esp_hid_gap.h"

#include "esp_bt_main.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "esp_hid_gap";

// One binary semaphore covers both scan-setup waits: SCAN_PARAM_SET_COMPLETE
// after esp_ble_gap_set_scan_params, and INQ_CMPL after the scan window ends.
static SemaphoreHandle_t s_scan_sem;
static esp_hid_scan_result_t *s_results;
static size_t s_num_results;

static esp_hid_scan_result_t *find_result(const esp_bd_addr_t bda) {
    for (esp_hid_scan_result_t *r = s_results; r != NULL; r = r->next) {
        if (memcmp(bda, r->bda, sizeof(esp_bd_addr_t)) == 0) {
            return r;
        }
    }
    return NULL;
}

static void add_result(const esp_bd_addr_t bda, esp_ble_addr_type_t addr_type, uint16_t appearance,
                       const uint8_t *name, uint8_t name_len, int rssi) {
    if (find_result(bda) != NULL) {
        return;
    }
    esp_hid_scan_result_t *r = malloc(sizeof(*r));
    if (r == NULL) {
        ESP_LOGE(TAG, "scan result alloc failed");
        return;
    }
    r->transport = ESP_HID_TRANSPORT_BLE;
    memcpy(r->bda, bda, sizeof(esp_bd_addr_t));
    r->ble.appearance = appearance;
    r->ble.addr_type = addr_type;
    r->usage = esp_hid_usage_from_appearance(appearance);
    r->rssi = rssi;
    r->name = NULL;
    if (name != NULL && name_len > 0) {
        char *copy = malloc((size_t)name_len + 1);
        if (copy == NULL) {
            free(r);
            ESP_LOGE(TAG, "scan result name alloc failed");
            return;
        }
        memcpy(copy, name, name_len);
        copy[name_len] = 0;
        r->name = copy;
    }
    r->next = s_results;
    s_results = r;
    s_num_results++;
}

// Keep only peers that advertise the HID service UUID; appearance + name are
// pulled if present so the caller can filter further (e.g. keyboard only).
static void handle_scan_result(struct ble_scan_result_evt_param *scan_rst) {
    uint16_t adv_total = scan_rst->adv_data_len + scan_rst->scan_rsp_len;

    uint8_t uuid_len = 0;
    uint8_t *uuid_d = esp_ble_resolve_adv_data_by_type(scan_rst->ble_adv, adv_total,
                                                       ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_len);
    if (uuid_d == NULL || uuid_len < 2) {
        return;
    }
    uint16_t uuid = (uint16_t)(uuid_d[0] | (uuid_d[1] << 8));
    if (uuid != ESP_GATT_UUID_HID_SVC) {
        return;
    }

    uint16_t appearance = 0;
    uint8_t appearance_len = 0;
    uint8_t *appearance_d = esp_ble_resolve_adv_data_by_type(
        scan_rst->ble_adv, adv_total, ESP_BLE_AD_TYPE_APPEARANCE, &appearance_len);
    if (appearance_d != NULL && appearance_len >= 2) {
        appearance = (uint16_t)(appearance_d[0] | (appearance_d[1] << 8));
    }

    uint8_t name_len = 0;
    uint8_t *name = esp_ble_resolve_adv_data_by_type(scan_rst->ble_adv, adv_total,
                                                     ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
    if (name == NULL) {
        name = esp_ble_resolve_adv_data_by_type(scan_rst->ble_adv, adv_total,
                                                ESP_BLE_AD_TYPE_NAME_SHORT, &name_len);
    }

    add_result(scan_rst->bda, scan_rst->ble_addr_type, appearance, name, name_len, scan_rst->rssi);
}

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        xSemaphoreGive(s_scan_sem);
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            handle_scan_result(&param->scan_rst);
        } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            xSemaphoreGive(s_scan_sem);
        }
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "pairing succeeded");
        } else {
            ESP_LOGE(TAG, "pairing failed: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        // Peer (the keyboard) must type this number to pair.
        ESP_LOGI(TAG, "passkey: %06" PRIu32, param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        // Numeric comparison: auto-accept; we have no UI to confirm.
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    default:
        ESP_LOGV(TAG, "ble gap event %d", event);
        break;
    }
}

static esp_err_t init_controller(void) {
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller_mem_release: %d", ret);
        return ret;
    }
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller_init: %d", ret);
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller_enable: %d", ret);
        return ret;
    }
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bd_cfg.ssp_en = false;
    ret = esp_bluedroid_init_with_cfg(&bd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid_init: %d", ret);
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid_enable: %d", ret);
        return ret;
    }
    return esp_ble_gap_register_callback(ble_gap_event_handler);
}

esp_err_t esp_hid_gap_init(uint8_t mode) {
    if (mode != ESP_BT_MODE_BLE) {
        ESP_LOGE(TAG, "only ESP_BT_MODE_BLE is supported");
        return ESP_FAIL;
    }
    if (s_scan_sem != NULL) {
        ESP_LOGE(TAG, "already initialised");
        return ESP_FAIL;
    }
    s_scan_sem = xSemaphoreCreateBinary();
    if (s_scan_sem == NULL) {
        return ESP_FAIL;
    }
    esp_err_t ret = init_controller();
    if (ret != ESP_OK) {
        vSemaphoreDelete(s_scan_sem);
        s_scan_sem = NULL;
    }
    return ret;
}

static esp_ble_scan_params_t s_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
};

esp_err_t esp_hid_scan(uint32_t seconds, size_t *num_results, esp_hid_scan_result_t **results) {
    if (s_results != NULL || s_num_results != 0) {
        ESP_LOGE(TAG, "previous scan results not freed");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_ble_gap_set_scan_params(&s_scan_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_scan_params: %d", ret);
        return ret;
    }
    xSemaphoreTake(s_scan_sem, portMAX_DELAY);

    ret = esp_ble_gap_start_scanning(seconds);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start_scanning: %d", ret);
        return ret;
    }
    xSemaphoreTake(s_scan_sem, portMAX_DELAY);

    *num_results = s_num_results;
    *results = s_results;
    s_num_results = 0;
    s_results = NULL;
    return ESP_OK;
}

void esp_hid_scan_results_free(esp_hid_scan_result_t *results) {
    while (results != NULL) {
        esp_hid_scan_result_t *r = results;
        results = results->next;
        if (r->name != NULL) {
            free((char *)r->name);
        }
        free(r);
    }
}
