// Test stanowiskowy OLED SH1106 (SDA=GPIO20, SCL=GPIO21, adres z app_config.h).
// 1) skan magistrali I2C i wypisanie znalezionych adresów,
// 2) inicjalizacja wyświetlacza i pokazanie przykładowych stanów radaru.
#include <inttypes.h>
#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "oled.h"

static const char *TAG = "OLED_BENCH";

static void scan_i2c(void)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = OLED_I2C_PORT,
        .sda_io_num = GPIO_OLED_SDA,
        .scl_io_num = GPIO_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "cannot create I2C bus for scan");
        return;
    }
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; ++addr) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
            ++found;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "no I2C devices found (check wiring/power)");
    }
    i2c_del_master_bus(bus);
}

void app_main(void)
{
    ESP_LOGI(TAG, "OLED bench test: SDA=GPIO%d, SCL=GPIO%d, configured address 0x%02X",
             GPIO_OLED_SDA, GPIO_OLED_SCL, OLED_I2C_ADDRESS);
    scan_i2c();

    const esp_err_t err = oled_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "oled_init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "oled_init OK, drawing test screens");

    const radar_state_t states[] = {RADAR_STATE_PATROL, RADAR_STATE_TARGET_LOCKED,
                                    RADAR_STATE_CRITICAL_ALERT, RADAR_STATE_TARGET_LOST};
    int step = 0;
    while (true) {
        radar_status_t s = {
            .state = states[(step / 4) % 4],
            .angle_deg = 15 + (step * 6) % 150,
            .distance_cm = 20 + (step * 7) % 100,
            .timestamp_ms = 0,
            .distance_valid = (step % 9) != 8,
        };
        const esp_err_t e = oled_show_status(&s);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "oled_show_status failed: %s", esp_err_to_name(e));
        }
        ++step;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
