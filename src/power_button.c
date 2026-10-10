#include "power_button.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "logic.h"

static const char *TAG = "POWER_BUTTON";

static void power_button_task(void *arg)
{
    (void)arg;

    uint32_t pressed_ms = 0;
    bool handled = false;

    while (true) {
        const bool pressed = gpio_get_level(GPIO_POWER_BUTTON) == 0;
        if (pressed) {
            pressed_ms += POWER_BUTTON_POLL_MS;
            if (!handled && pressed_ms >= POWER_BUTTON_DEBOUNCE_MS) {
                handled = true; /* one toggle per press, until the button is released */
                const bool now_on = !logic_is_power_enabled();
                logic_set_power_enabled(now_on);
                ESP_LOGI(TAG, "Button: radar %s", now_on ? "ON" : "standby");
            }
        } else {
            pressed_ms = 0;
            handled = false;
        }
        vTaskDelay(pdMS_TO_TICKS(POWER_BUTTON_POLL_MS));
    }
}

esp_err_t power_button_start(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << GPIO_POWER_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    if (xTaskCreate(power_button_task, "power_button", RADAR_POWER_TASK_STACK_SIZE,
                    NULL, RADAR_POWER_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
