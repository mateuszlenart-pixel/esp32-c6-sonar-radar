#include "radar.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "logic.h"
#include "scan_control.h"
#include "sonar_task.h"
#include "servo.h"

static const char *TAG = "RADAR";

static void radar_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    int angle = RADAR_MIN_ANGLE_DEG;
    int direction = 1;
    bool has_scanned = false;

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_STEP_PERIOD_MS));

        if (has_scanned) {
            const int previous_angle = angle;
            angle = radar_scan_advance(angle,
                                       &direction,
                                       logic_is_target_locked(),
                                       RADAR_MIN_ANGLE_DEG,
                                       RADAR_MAX_ANGLE_DEG,
                                       RADAR_STEP_DEGREES);
            if (angle == RADAR_MAX_ANGLE_DEG && angle != previous_angle) {
                ESP_LOGI(TAG, "Patrol reached %d degrees", angle);
            } else if (angle == RADAR_MIN_ANGLE_DEG && angle != previous_angle) {
                ESP_LOGI(TAG, "Patrol reached %d degrees", angle);
            }
        }

        ESP_ERROR_CHECK(servo_set_angle(angle));
        vTaskDelay(pdMS_TO_TICKS(RADAR_SERVO_SETTLE_MS));

        if (!sonar_task_request_measurement(angle)) {
            ESP_LOGW(TAG, "Sonar request queue full; measurement skipped");
        }
        has_scanned = true;
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
