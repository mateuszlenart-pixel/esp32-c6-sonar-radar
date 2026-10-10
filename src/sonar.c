#include "sonar.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "app_config.h"

// The ECHO pulse width is timestamped by the MCPWM capture hardware, so interrupts and task
// switches (e.g. the Zigbee radio) cannot stretch the measurement as a polling loop would.

static bool s_initialized;
static mcpwm_cap_timer_handle_t s_cap_timer;
static mcpwm_cap_channel_handle_t s_cap_channel;
static SemaphoreHandle_t s_echo_done;
static uint32_t s_ticks_per_us;
static volatile uint32_t s_rise_ticks;
static volatile uint32_t s_width_ticks;
static volatile bool s_rise_seen;

static bool on_echo_edge(mcpwm_cap_channel_handle_t channel,
                         const mcpwm_capture_event_data_t *edata,
                         void *user_ctx)
{
    (void)channel;
    (void)user_ctx;
    BaseType_t woken = pdFALSE;
    // Noise from the LED matrix couples sub-microsecond spikes into ECHO. An edge counts only if
    // the line still holds the new level when the interrupt runs, and a pulse shorter than
    // SONAR_MIN_ECHO_US is not an echo; in both cases keep waiting for the real pulse.
    const int level = gpio_get_level(GPIO_SONAR_ECHO);
    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
        // A dip during the echo must not restart the pulse, so the first valid rise is kept.
        if (level == 1 && !s_rise_seen) {
            s_rise_ticks = edata->cap_value;
            s_rise_seen = true;
        }
    } else if (s_rise_seen && level == 0) {
        const uint32_t width = edata->cap_value - s_rise_ticks;
        if (width >= SONAR_MIN_ECHO_US * s_ticks_per_us) {
            s_width_ticks = width;
            s_rise_seen = false;
            xSemaphoreGiveFromISR(s_echo_done, &woken);
        }
    }
    return woken == pdTRUE;
}

static bool wait_for_echo_low(uint32_t timeout_us)
{
    const int64_t start_us = esp_timer_get_time();
    while (gpio_get_level(GPIO_SONAR_ECHO) != 0) {
        if ((uint64_t)(esp_timer_get_time() - start_us) >= timeout_us) {
            return false;
        }
        esp_rom_delay_us(SONAR_POLL_INTERVAL_US);
    }
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
    err = gpio_set_level(GPIO_SONAR_TRIG, 0);
    if (err != ESP_OK) {
        return err;
    }

    s_echo_done = xSemaphoreCreateBinary();
    if (s_echo_done == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const mcpwm_capture_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
    };
    err = mcpwm_new_capture_timer(&timer_config, &s_cap_timer);
    if (err != ESP_OK) {
        return err;
    }

    const mcpwm_capture_channel_config_t channel_config = {
        .gpio_num = GPIO_SONAR_ECHO,
        .prescale = 1,
        .flags.pos_edge = true,
        .flags.neg_edge = true,
    };
    err = mcpwm_new_capture_channel(s_cap_timer, &channel_config, &s_cap_channel);
    if (err != ESP_OK) {
        return err;
    }
    // The divider (15 kΩ to GND) already holds ECHO low; the internal pull-down only helps
    // when the sonar is unplugged.
    (void)gpio_pulldown_en(GPIO_SONAR_ECHO);

    const mcpwm_capture_event_callbacks_t callbacks = {
        .on_cap = on_echo_edge,
    };
    err = mcpwm_capture_channel_register_event_callbacks(s_cap_channel, &callbacks, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = mcpwm_capture_channel_enable(s_cap_channel);
    if (err != ESP_OK) {
        return err;
    }
    err = mcpwm_capture_timer_enable(s_cap_timer);
    if (err != ESP_OK) {
        return err;
    }
    err = mcpwm_capture_timer_start(s_cap_timer);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t resolution_hz = 0;
    err = mcpwm_capture_timer_get_resolution(s_cap_timer, &resolution_hz);
    if (err != ESP_OK) {
        return err;
    }
    s_ticks_per_us = resolution_hz / 1000000U;
    if (s_ticks_per_us == 0) {
        return ESP_ERR_INVALID_STATE;
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

    if (!wait_for_echo_low(SONAR_RISE_TIMEOUT_US)) {
        return ESP_ERR_TIMEOUT;
    }

    s_rise_seen = false;
    (void)xSemaphoreTake(s_echo_done, 0);

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

    const uint32_t timeout_ms = (SONAR_RISE_TIMEOUT_US + SONAR_ECHO_TIMEOUT_US) / 1000U + 1U;
    if (xSemaphoreTake(s_echo_done, pdMS_TO_TICKS(timeout_ms) + 1) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    const uint32_t pulse_width_us = s_width_ticks / s_ticks_per_us;
    if (pulse_width_us > SONAR_ECHO_TIMEOUT_US) {
        return ESP_ERR_TIMEOUT;
    }
    *distance_cm = (pulse_width_us + 29U) / 58U;
    return ESP_OK;
}
