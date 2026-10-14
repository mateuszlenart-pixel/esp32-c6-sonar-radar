// Test stanowiskowy HC-SR04 (tylko TRIG GPIO11 i ECHO GPIO6, bez serwa/OLED/matrycy/sieci).
// Co 500 ms wykonuje pomiar i wypisuje wynik przez port szeregowy.
#include <inttypes.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "sonar.h"

static const char *TAG = "SONAR_BENCH";

void app_main(void)
{
    ESP_LOGI(TAG, "HC-SR04 bench test: TRIG=GPIO%d, ECHO=GPIO%d", GPIO_SONAR_TRIG, GPIO_SONAR_ECHO);
    ESP_ERROR_CHECK(sonar_init());

    while (true) {
        uint32_t cm = 0;
        const esp_err_t err = sonar_measure_cm(&cm);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "distance = %" PRIu32 " cm", cm);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "no echo (timeout)");
        } else {
            ESP_LOGE(TAG, "measure failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
