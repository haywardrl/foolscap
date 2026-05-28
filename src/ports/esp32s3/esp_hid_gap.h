/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Trimmed from the ESP-IDF esp_hid_host example to the BLE-only Bluedroid
 * paths that foolscap actually uses: scan for HID peripherals, hand the
 * results to esp_hidh.
 */

#pragma once

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_hid_common.h"

typedef struct esp_hidh_scan_result_s {
    struct esp_hidh_scan_result_s *next;
    esp_bd_addr_t bda;
    const char *name;
    int8_t rssi;
    esp_hid_usage_t usage;
    esp_hid_transport_t transport;
    struct {
        esp_ble_addr_type_t addr_type;
        uint16_t appearance;
    } ble;
} esp_hid_scan_result_t;

// Bring up the BT controller, Bluedroid, and register the BLE GAP callback.
// `mode` must be ESP_BT_MODE_BLE.
esp_err_t esp_hid_gap_init(uint8_t mode);

// Active scan for `seconds`, returning a linked list of HID-service-advertising
// peers. Caller takes ownership and must free with esp_hid_scan_results_free.
esp_err_t esp_hid_scan(uint32_t seconds, size_t *num_results, esp_hid_scan_result_t **results);

void esp_hid_scan_results_free(esp_hid_scan_result_t *results);
