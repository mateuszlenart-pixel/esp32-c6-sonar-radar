// Test stanowiskowy: odczyt z HC-SR04 wyświetlany na OLED-zie.
// Bez serwa: kąt na ekranie jest stały (90), stan wynika wyłącznie z odległości
// i progów z app_config.h (bez histerezy i potwierdzeń, w odróżnieniu od pełnego firmware).
#include <inttypes.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "oled.h"
#include "sonar.h"

static const char *TAG = "SONAR_OLED";

void app_main(void)
{
    ESP_LOGI(TAG, "Sonar + OLED bench: TRIG=GPIO%d ECHO=GPIO%d SDA=GPIO%d SCL=GPIO%d",
             GPIO_SONAR_TRIG, GPIO_SONAR_ECHO, GPIO_OLED_SDA, GPIO_OLED_SCL);
    ESP_ERROR_CHECK(sonar_init());
    ESP_ERROR_CHECK(oled_init());

    while (true) {
        uint32_t cm = 0;
        const esp_err_t err = sonar_measure_cm(&cm);
        const bool valid = (err == ESP_OK);

        radar_status_t s = {
            .state = RADAR_STATE_PATROL,
            .angle_deg = 90,
            .distance_cm = valid ? cm : 0,
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
            .distance_valid = valid,
        };
        if (valid && cm <= RADAR_CRITICAL_DISTANCE_CM) {
            s.state = RADAR_STATE_CRITICAL_ALERT;
        } else if (valid && cm <= RADAR_TARGET_DISTANCE_CM) {
            s.state = RADAR_STATE_TARGET_LOCKED;
        }

        if (valid) {
            ESP_LOGI(TAG, "distance = %" PRIu32 " cm", cm);
        } else {
            ESP_LOGW(TAG, "no echo (%s)", esp_err_to_name(err));
        }
        const esp_err_t oe = oled_show_status(&s);
        if (oe != ESP_OK) {
            ESP_LOGE(TAG, "oled_show_status failed: %s", esp_err_to_name(oe));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
