#include "sonar.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "app_config.h"

static bool s_initialized;

static bool wait_for_echo_level(int level, uint32_t timeout_us, int64_t *timestamp_us)
{
    const int64_t start_us = esp_timer_get_time();

    while (gpio_get_level(GPIO_SONAR_ECHO) != level) {
        if ((uint64_t)(esp_timer_get_time() - start_us) >= timeout_us) {
            return false;
        }
        esp_rom_delay_us(SONAR_POLL_INTERVAL_US);
    }

    *timestamp_us = esp_timer_get_time();
    return true;
}

esp_err_t sonar_init(void)
{
    const gpio_config_t trigger_config = {
        .pin_bit_mask = 1ULL << GPIO_SONAR_TRIG,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&trigger_config);
    if (err != ESP_OK) {
        return err;
    }

    const gpio_config_t echo_config = {
        .pin_bit_mask = 1ULL << GPIO_SONAR_ECHO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&echo_config);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(GPIO_SONAR_TRIG, 0);
    if (err != ESP_OK) {
        return err;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t sonar_measure_cm(uint32_t *distance_cm)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (distance_cm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t ignored_timestamp_us = 0;
    if (!wait_for_echo_level(0, SONAR_RISE_TIMEOUT_US, &ignored_timestamp_us)) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = gpio_set_level(GPIO_SONAR_TRIG, 0);
    if (err != ESP_OK) {
        return err;
    }
    esp_rom_delay_us(2);

    err = gpio_set_level(GPIO_SONAR_TRIG, 1);
    if (err != ESP_OK) {
        return err;
    }
    esp_rom_delay_us(SONAR_TRIGGER_PULSE_US);

    err = gpio_set_level(GPIO_SONAR_TRIG, 0);
    if (err != ESP_OK) {
        return err;
    }

    int64_t echo_start_us = 0;
    if (!wait_for_echo_level(1, SONAR_RISE_TIMEOUT_US, &echo_start_us)) {
        return ESP_ERR_TIMEOUT;
    }

    int64_t echo_end_us = 0;
    if (!wait_for_echo_level(0, SONAR_ECHO_TIMEOUT_US, &echo_end_us)) {
        return ESP_ERR_TIMEOUT;
    }

    const uint32_t pulse_width_us = (uint32_t)(echo_end_us - echo_start_us);
    *distance_cm = (pulse_width_us + 29U) / 58U;
    return ESP_OK;
}
