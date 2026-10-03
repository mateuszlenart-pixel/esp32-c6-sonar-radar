#include "buzzer.h"

#include <stdbool.h>
#include <stdint.h>

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
}

esp_err_t buzzer_silence(void)
{
    if (!s_is_on) {
        return ESP_OK;
    }

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL);
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
    ledc_stop(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CHANNEL, 0);
    s_initialized = false;
    s_is_on = false;
    s_current_frequency = 0;
}
