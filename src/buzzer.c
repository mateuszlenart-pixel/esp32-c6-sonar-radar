#include "buzzer.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "app_config.h"

#define BUZZER_PWM_TIMER LEDC_TIMER_1
#define BUZZER_PWM_CHANNEL LEDC_CHANNEL_1
#define BUZZER_MAX_DUTY ((1U << BUZZER_PWM_RESOLUTION_BITS) - 1U)

static bool s_initialized;
static bool s_is_on;
static uint32_t s_current_frequency;

static esp_err_t set_tone(uint32_t frequency_hz)
{
#if BUZZER_ACTIVE
    (void)frequency_hz; /* an active buzzer has a fixed tone; only the on/off pattern differs */
    const esp_err_t gpio_err = gpio_set_level(GPIO_BUZZER, 1);
    if (gpio_err == ESP_OK) {
        s_is_on = true;
    }
    return gpio_err;
#else
    esp_err_t err = ESP_OK;
    if (s_current_frequency != frequency_hz) {
        err = ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_PWM_TIMER, frequency_hz);
        if (err != ESP_OK) {
            return err;
        }
        s_current_frequency = frequency_hz;
    }

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE,
                        BUZZER_PWM_CHANNEL,
                        BUZZER_MAX_DUTY / 2U);
    if (err != ESP_OK) {
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL);
    if (err == ESP_OK) {
        s_is_on = true;
    }
    return err;
#endif
}

esp_err_t buzzer_silence(void)
{
    if (!s_is_on) {
        return ESP_OK;
    }

#if BUZZER_ACTIVE
    const esp_err_t err = gpio_set_level(GPIO_BUZZER, 0);
#else
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL);
#endif
    if (err == ESP_OK) {
        s_is_on = false;
    }
    return err;
}

esp_err_t buzzer_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

#if BUZZER_ACTIVE
    const gpio_config_t buzzer_pin = {
        .pin_bit_mask = 1ULL << GPIO_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&buzzer_pin);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(GPIO_BUZZER, 0);
#else
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)BUZZER_PWM_RESOLUTION_BITS,
        .timer_num = BUZZER_PWM_TIMER,
        .freq_hz = BUZZER_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        return err;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BUZZER_PWM_CHANNEL,
        .timer_sel = BUZZER_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        return err;
    }
#endif

    s_initialized = true;
    return ESP_OK;
}

esp_err_t buzzer_update(const radar_status_t *status, uint32_t now_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!status->distance_valid || status->state == RADAR_STATE_PATROL ||
        status->state == RADAR_STATE_TARGET_LOST) {
        return buzzer_silence();
    }

    if (status->state == RADAR_STATE_CRITICAL_ALERT) {
        const uint32_t phase_ms = now_ms % 300U;
        if (phase_ms >= 240U) {
            return buzzer_silence();
        }
        return set_tone(phase_ms < 120U ? 2600U : 1800U);
    }

    uint32_t interval_ms = 800U;
    if (status->distance_cm <= 30U) {
        interval_ms = 250U;
    } else if (status->distance_cm <= 40U) {
        interval_ms = 400U;
    }

    if (now_ms % interval_ms >= 80U) {
        return buzzer_silence();
    }
    return set_tone(1400U);
}

void buzzer_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    buzzer_silence();
#if !BUZZER_ACTIVE
    ledc_stop(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL, 0);
#endif
    s_initialized = false;
    s_is_on = false;
    s_current_frequency = 0;
}
