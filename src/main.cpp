#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core/AshuraCore.h"

static const char* MAIN_TAG = "main";

// Static allocation — AshuraCore is large, 
// heap allocation avoids stack overflow in app_main
static AshuraCore core;

static void main_task(void* arg) {
    ESP_LOGI(MAIN_TAG, "Main task started on core %d", xPortGetCoreID());

    core.init();

    while (true) {
        core.update();
        // Yield for 1ms — prevents watchdog trigger
        // and lets WiFi/BT stack tasks breathe
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Should never reach here
    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    // Pin UI + networking to Core 1
    // Core 0 is reserved for audio pipeline (future)
    xTaskCreatePinnedToCore(
        main_task,              // Function to run
        "main_task",            // Task name (for debugging)
        16384,                  // 16KB stack — AshuraCore is deep
        nullptr,                // No parameters
        5,                      // Priority 5 — above idle, below WiFi internals
        nullptr,                // Task handle (not used)
        1                       // Core 1 (ID)
    );
}