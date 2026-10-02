#include "radar.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "detector.h"
#include "sonar.h"
#include "servo.h"

static const char *TAG = "RADAR";

static void radar_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    int angle = RADAR_MIN_ANGLE_DEG;
    int direction = 1;
    radar_detector_t detector;
    radar_detector_init(&detector);

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_STEP_PERIOD_MS));

        ESP_ERROR_CHECK(servo_set_angle(angle));
        vTaskDelay(pdMS_TO_TICKS(RADAR_SERVO_SETTLE_MS));

        uint32_t distance_cm = 0;
        esp_err_t err = sonar_measure_cm(&distance_cm);
        const bool measurement_valid = err == ESP_OK;
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const radar_detector_events_t events = radar_detector_update(
            &detector, measurement_valid, distance_cm, now_ms);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "angle=%d deg, distance=%lu cm",
                     angle, (unsigned long)distance_cm);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "No echo at %d deg", angle);
        } else {
            ESP_LOGE(TAG, "Sonar measurement failed at %d deg: %s",
                     angle, esp_err_to_name(err));
        }

        if (events & RADAR_EVENT_TARGET_ACQUIRED) {
            ESP_LOGI(TAG, "Target detected at %d deg, %lu cm",
                     angle, (unsigned long)distance_cm);
        }
        if (events & RADAR_EVENT_CRITICAL_ENTERED) {
            ESP_LOGW(TAG, "CRITICAL: target at %d deg, %lu cm",
                     angle, (unsigned long)distance_cm);
        }
        if (events & RADAR_EVENT_CRITICAL_CLEARED) {
            ESP_LOGI(TAG, "Critical distance alert cleared");
        }
        if (events & RADAR_EVENT_TARGET_LOST) {
            ESP_LOGI(TAG, "Target lost");
        }

        if (!detector.target_active) {
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
