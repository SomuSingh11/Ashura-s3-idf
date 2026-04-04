#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// This tells the compiler: "Keep the name 'app_main' exactly as it is"
extern "C" {
    void app_main(void);
}

void app_main(void)
{
    printf("HELLO FROM APP_MAIN\n");

    while (1) {
        printf("Heartbeat...\n");
        // Yielding for 2 seconds to keep the watchdog happy
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}