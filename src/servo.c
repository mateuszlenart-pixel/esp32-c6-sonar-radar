#include "servo.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/ledc.h"

#include "app_config.h"

#define SERVO_PWM_PERIOD_US (1000000U / SERVO_PWM_FREQUENCY_HZ)
#define SERVO_DUTY_MAX ((1U << SERVO_PWM_RESOLUTION_BITS) - 1U)

static bool s_initialized;

esp_err_t servo_set_angle(int angle_deg)
{
    if (!s_initialized || angle_deg < RADAR_MIN_ANGLE_DEG || angle_deg > RADAR_MAX_ANGLE_DEG) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t angle_span = RADAR_MAX_ANGLE_DEG - RADAR_MIN_ANGLE_DEG;
    const uint32_t pulse_span = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    const uint32_t pulse_us = SERVO_MIN_PULSE_US +
        ((uint32_t)(angle_deg - RADAR_MIN_ANGLE_DEG) * pulse_span) / angle_span;
    const uint32_t duty = (pulse_us * SERVO_DUTY_MAX) / SERVO_PWM_PERIOD_US;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

esp_err_t servo_release(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Stop the PWM pulses: the servo stops holding position and draws no current. */
    return ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
}

esp_err_t servo_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)SERVO_PWM_RESOLUTION_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        return err;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = GPIO_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        return err;
    }

    s_initialized = true;
    return servo_set_angle(RADAR_MIN_ANGLE_DEG);
}
