#include "ui_task.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "buzzer.h"
#include "logic.h"
#include "matrix.h"
#include "oled.h"

static const char *TAG = "UI";
static bool s_oled_ready;
static bool s_matrix_ready;
static bool s_buzzer_ready;
static bool s_was_powered = true;

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
    radar_status_t previous_oled_status = {0};
    uint32_t last_oled_update_ms = 0;
    bool has_oled_status = false;

    while (true) {
        if (!logic_is_power_enabled()) {
            if (s_was_powered) {
                s_was_powered = false;
                if (s_buzzer_ready) {
                    (void)buzzer_silence();
                }
                if (s_oled_ready) {
                    (void)oled_set_power(false);
                }
                if (s_matrix_ready) {
                    (void)matrix_set_power(false);
                }
                ESP_LOGI(TAG, "Standby: displays off, buzzer silent");
            }
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_UI_REFRESH_MS));
            continue;
        }
        if (!s_was_powered) {
            s_was_powered = true;
            has_oled_status = false;
            if (s_oled_ready) {
                (void)oled_set_power(true);
            }
            if (s_matrix_ready) {
                (void)matrix_set_power(true);
            }
            ESP_LOGI(TAG, "Power on: displays resumed");
        }

        radar_status_t current;
        if (logic_get_status(&current)) {
            const uint32_t now_ms = (uint32_t)(xTaskGetTickCount() *
                                               portTICK_PERIOD_MS);
            if (s_buzzer_ready) {
                const esp_err_t err = logic_is_buzzer_enabled()
                    ? buzzer_update(&current, now_ms)
                    : buzzer_silence();
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Buzzer update failed: %s", esp_err_to_name(err));
                    buzzer_deinit();
                    s_buzzer_ready = false;
                }
            }

            if (s_oled_ready &&
                (!has_oled_status ||
                 ((uint32_t)(now_ms - last_oled_update_ms) >= 200U &&
                  status_changed(&current, &previous_oled_status)))) {
                const esp_err_t err = oled_show_status(&current);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OLED update failed: %s", esp_err_to_name(err));
                    oled_deinit();
                    s_oled_ready = false;
                } else {
                    previous_oled_status = current;
                    last_oled_update_ms = now_ms;
                    has_oled_status = true;
                }
            }

            if (s_matrix_ready) {
                const esp_err_t err = matrix_render_status(&current, now_ms);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Matrix update failed: %s", esp_err_to_name(err));
                    s_matrix_ready = false;
                }
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RADAR_UI_REFRESH_MS));
    }
}

esp_err_t ui_task_start(void)
{
    const esp_err_t oled_err = oled_init();
    s_oled_ready = oled_err == ESP_OK;
    if (!s_oled_ready) {
        ESP_LOGW(TAG,
                 "OLED unavailable; radar will continue without display: %s",
                 esp_err_to_name(oled_err));
    }

    const esp_err_t matrix_err = matrix_init();
    s_matrix_ready = matrix_err == ESP_OK;
    if (!s_matrix_ready) {
        ESP_LOGW(TAG,
                 "Matrix unavailable; radar will continue without matrix: %s",
                 esp_err_to_name(matrix_err));
    }

    const esp_err_t buzzer_err = buzzer_init();
    s_buzzer_ready = buzzer_err == ESP_OK;
    if (!s_buzzer_ready) {
        ESP_LOGW(TAG,
                 "Buzzer unavailable; radar will continue silently: %s",
                 esp_err_to_name(buzzer_err));
    }

    if (!s_oled_ready && !s_matrix_ready && !s_buzzer_ready) {
        return ESP_OK;
    }

    if (xTaskCreate(ui_task,
                    "ui_task",
                    RADAR_UI_TASK_STACK_SIZE,
                    NULL,
                    RADAR_UI_TASK_PRIORITY,
                    NULL) != pdPASS) {
        if (s_oled_ready) {
            oled_deinit();
            s_oled_ready = false;
        }
        if (s_buzzer_ready) {
            buzzer_deinit();
            s_buzzer_ready = false;
        }
        ESP_LOGE(TAG, "Failed to create UI task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
