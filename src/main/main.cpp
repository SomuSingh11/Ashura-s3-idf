#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core/AshuraCore.h"

static const char* MAIN_TAG = "main";
static AshuraCore core;

// Static allocation — AshuraCore is large, 
// heap allocation avoids stack overflow in app_main
// static void main_task(void* arg) {
//     ESP_LOGI(MAIN_TAG, "main_task started on core %d", xPortGetCoreID());
//     vTaskDelay(pdMS_TO_TICKS(1000));
//     while (true) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
// extern "C" void app_main() {
//     ESP_LOGI(MAIN_TAG, "app_main entered");

//     BaseType_t ok = xTaskCreatePinnedToCore(
//         main_task,
//         "main_task",
//         16384,
//         nullptr,
//         5,
//         nullptr,
//         1
//     );

//     ESP_LOGI(MAIN_TAG, "xTaskCreatePinnedToCore returned %d", (int)ok);
// }



static void main_task(void* arg) {
    ESP_LOGI(MAIN_TAG, "Main task started on core %d", xPortGetCoreID());
    
    core.init();

    while (true) {
        core.update();
        // Yield for 1ms — prevents watchdog trigger
        // and lets WiFi/BT stack tasks breathe
        vTaskDelay(pdMS_TO_TICKS(10));
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
        32768,                  // 16KB stack — AshuraCore is deep
        nullptr,                // No parameters
        5,                      // Priority 5 — above idle, below WiFi internals
        nullptr,                // Task handle (not used)
        1                       // Core 1 (ID)
    );
}