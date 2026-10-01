#include "radar.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "servo.h"

static const char *TAG = "RADAR";

static void radar_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    int angle = RADAR_MIN_ANGLE_DEG;
    int direction = 1;

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_STEP_PERIOD_MS));

        angle += direction * RADAR_STEP_DEGREES;
        if (angle >= RADAR_MAX_ANGLE_DEG) {
            angle = RADAR_MAX_ANGLE_DEG;
            direction = -1;
            ESP_LOGI(TAG, "Patrol reached %d degrees", angle);
        } else if (angle <= RADAR_MIN_ANGLE_DEG) {
            angle = RADAR_MIN_ANGLE_DEG;
            direction = 1;
            ESP_LOGI(TAG, "Patrol reached %d degrees", angle);
        }

        ESP_ERROR_CHECK(servo_set_angle(angle));
    }
}

void radar_start(void)
{
    BaseType_t result = xTaskCreate(radar_task,
                                    "radar_task",
                                    RADAR_TASK_STACK_SIZE,
                                    NULL,
                                    RADAR_TASK_PRIORITY,
                                    NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create radar task");
        abort();
    }
}
