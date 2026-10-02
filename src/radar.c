#include "radar.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "sonar.h"
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

        ESP_ERROR_CHECK(servo_set_angle(angle));
        vTaskDelay(pdMS_TO_TICKS(RADAR_SERVO_SETTLE_MS));

        uint32_t distance_cm = 0;
        esp_err_t err = sonar_measure_cm(&distance_cm);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "angle=%d deg, distance=%lu cm",
                     angle, (unsigned long)distance_cm);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "No echo at %d deg", angle);
        } else {
            ESP_LOGE(TAG, "Sonar measurement failed at %d deg: %s",
                     angle, esp_err_to_name(err));
        }

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
