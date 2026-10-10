#include "sonar_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_config.h"
#include "logic.h"
#include "matrix.h"
#include "sonar.h"

static const char *TAG = "SONAR_TASK";
static QueueHandle_t s_request_queue;
static uint32_t s_history[SONAR_MEDIAN_WINDOW];
static int s_history_count;
static int s_history_next;

// Median of the last SONAR_MEDIAN_WINDOW valid readings: one or two isolated false echoes
// (e.g. from supply noise) do not switch the radar state.
static uint32_t median_filter(uint32_t value)
{
    s_history[s_history_next] = value;
    s_history_next = (s_history_next + 1) % SONAR_MEDIAN_WINDOW;
    if (s_history_count < SONAR_MEDIAN_WINDOW) {
        ++s_history_count;
    }
    if (s_history_count < 3) {
        return value;
    }
    uint32_t sorted[SONAR_MEDIAN_WINDOW];
    for (int i = 0; i < s_history_count; ++i) {
        sorted[i] = s_history[i];
    }
    for (int i = 1; i < s_history_count; ++i) {
        const uint32_t key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            --j;
        }
        sorted[j + 1] = key;
    }
    return sorted[s_history_count / 2];
}

static void sonar_task(void *arg)
{
    (void)arg;

    int angle_deg;
    while (xQueueReceive(s_request_queue, &angle_deg, portMAX_DELAY) == pdTRUE) {
        if (!logic_is_power_enabled()) {
            continue;
        }
        uint32_t distance_cm = 0;
        matrix_quiet(true);
        const esp_err_t err = sonar_measure_cm(&distance_cm);
        matrix_quiet(false);
        const bool distance_valid = err == ESP_OK;
        const uint32_t raw_cm = distance_cm;
        if (distance_valid) {
            distance_cm = median_filter(raw_cm);
        }
        const uint32_t timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

        if (!logic_submit_measurement(angle_deg,
                                      distance_valid,
                                      distance_cm,
                                      timestamp_ms)) {
            ESP_LOGW(TAG, "Logic queue full; dropping measurement");
        }

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "angle=%d deg, distance=%lu cm (raw %lu)",
                     angle_deg, (unsigned long)distance_cm, (unsigned long)raw_cm);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "No echo at %d deg", angle_deg);
        } else {
            ESP_LOGE(TAG, "Measurement failed at %d deg: %s",
                     angle_deg, esp_err_to_name(err));
        }
    }
}

esp_err_t sonar_task_start(void)
{
    if (s_request_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_request_queue = xQueueCreate(SONAR_REQUEST_QUEUE_LENGTH, sizeof(int));
    if (s_request_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(sonar_task,
                    "sonar_task",
                    RADAR_SONAR_TASK_STACK_SIZE,
                    NULL,
                    RADAR_SONAR_TASK_PRIORITY,
                    NULL) != pdPASS) {
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
        ESP_LOGE(TAG, "Failed to create sonar task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool sonar_task_request_measurement(int angle_deg)
{
    if (s_request_queue == NULL ||
        angle_deg < RADAR_MIN_ANGLE_DEG || angle_deg > RADAR_MAX_ANGLE_DEG) {
        return false;
    }

    return xQueueSend(s_request_queue, &angle_deg, 0) == pdTRUE;
}
