#pragma once

// ============================================================
//  Hardware Abstraction Layer
// ============================================================

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Drop-in millis() replacement for IDF
static inline uint64_t millis() {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

// Drop-in delay() replacement for IDF
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Drop-in yield() replacement
static inline void yield() {
    taskYIELD();
}