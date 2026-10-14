// Test stanowiskowy matrycy MAX7219: cyklicznie pokazuje wszystkie stany radaru
// (jasność z MAX7219_INTENSITY w app_config.h, czyli niska).
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "matrix.h"

static const char *TAG = "MATRIX_BENCH";

void app_main(void)
{
    ESP_LOGI(TAG, "MAX7219 bench test: DIN=GPIO%d, CLK=GPIO%d, CS=GPIO%d, intensity=%d",
             GPIO_MAX7219_DIN, GPIO_MAX7219_CLK, GPIO_MAX7219_CS, MAX7219_INTENSITY);
    ESP_ERROR_CHECK(matrix_init());

    const radar_state_t states[] = {RADAR_STATE_PATROL, RADAR_STATE_TARGET_LOCKED,
                                    RADAR_STATE_CRITICAL_ALERT, RADAR_STATE_TARGET_LOST};
    while (true) {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const radar_status_t s = {
            .state = states[(now_ms / 4000U) % 4U],
            .angle_deg = 15 + (int)((now_ms / 100U) % 150U),
            .distance_cm = 40,
            .timestamp_ms = now_ms,
            .distance_valid = true,
        };
        const esp_err_t e = matrix_render_status(&s, now_ms);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "render failed: %s", esp_err_to_name(e));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
