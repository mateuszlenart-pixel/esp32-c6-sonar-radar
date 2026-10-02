#include "ui_task.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "logic.h"
#include "oled.h"

static const char *TAG = "UI";

static bool status_changed(const radar_status_t *left, const radar_status_t *right)
{
    return left->state != right->state ||
           left->angle_deg != right->angle_deg ||
           left->distance_cm != right->distance_cm ||
           left->distance_valid != right->distance_valid;
}

static void ui_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    radar_status_t previous = {0};
    bool has_previous = false;

    while (true) {
        radar_status_t current;
        if (logic_get_status(&current) &&
            (!has_previous || status_changed(&current, &previous))) {
            const esp_err_t err = oled_show_status(&current);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OLED update failed: %s", esp_err_to_name(err));
                oled_deinit();
                vTaskDelete(NULL);
            }
            previous = current;
            has_previous = true;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_UI_REFRESH_MS));
    }
}

esp_err_t ui_task_start(void)
{
    esp_err_t err = oled_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "OLED unavailable; radar will continue without display: %s",
                 esp_err_to_name(err));
        return ESP_OK;
    }

    if (xTaskCreate(ui_task,
                    "ui_task",
                    RADAR_UI_TASK_STACK_SIZE,
                    NULL,
                    RADAR_UI_TASK_PRIORITY,
                    NULL) != pdPASS) {
        oled_deinit();
        ESP_LOGE(TAG, "Failed to create UI task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
