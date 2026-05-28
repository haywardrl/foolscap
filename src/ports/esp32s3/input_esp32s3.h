#pragma once

#include "key_event.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Start the BLE HID keyboard input source. Decoded key events are posted to `out`.
void input_start(QueueHandle_t out);
