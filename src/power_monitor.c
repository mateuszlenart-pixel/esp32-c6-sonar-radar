#include "power_monitor.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include "app_config.h"

static const char *TAG = "POWER_MON";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static adc_channel_t s_ch_v5;
static adc_channel_t s_ch_cc1;
static adc_channel_t s_ch_cc2;
static power_status_t s_status;
static bool s_status_valid;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t read_mv(adc_channel_t channel, uint32_t *mv)
{
    int raw_sum = 0;
    for (int i = 0; i < POWER_MONITOR_SAMPLES; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_adc, channel, &raw);
        if (err != ESP_OK) {
            return err;
        }
        raw_sum += raw;
    }
    const int raw_avg = raw_sum / POWER_MONITOR_SAMPLES;
    int value_mv = 0;
    if (s_cali != NULL) {
        esp_err_t err = adc_cali_raw_to_voltage(s_cali, raw_avg, &value_mv);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        /* No calibration data: rough linear conversion for 12 dB attenuation (about 0-3.1 V). */
        value_mv = (raw_avg * 3100) / 4095;
    }
    *mv = value_mv < 0 ? 0U : (uint32_t)value_mv;
    return ESP_OK;
}

static esp_err_t sample(power_status_t *out)
{
    uint32_t v5_pin_mv = 0;
    esp_err_t err = read_mv(s_ch_v5, &v5_pin_mv);
    if (err != ESP_OK) {
        return err;
    }
    err = read_mv(s_ch_cc1, &out->cc1_mv);
    if (err != ESP_OK) {
        return err;
    }
    err = read_mv(s_ch_cc2, &out->cc2_mv);
    if (err != ESP_OK) {
        return err;
    }
    out->supply_mv = usb_divider_to_supply_mv(v5_pin_mv, V5_DIVIDER_TOP_KOHM, V5_DIVIDER_BOTTOM_KOHM);
    out->supply = usb_cc_best(usb_cc_classify(out->cc1_mv), usb_cc_classify(out->cc2_mv));
    out->overcurrent = gpio_get_level(GPIO_POWER_FAULT) == 0;
    return ESP_OK;
}

static void power_monitor_task(void *arg)
{
    (void)arg;

    uint32_t seconds_since_report = POWER_MONITOR_REPORT_S; /* report right after start */
    usb_cc_current_t last_supply = USB_CC_NONE;
    bool last_overcurrent = false;
    bool last_low = false;
    bool first = true;

    while (true) {
        power_status_t now = {0};
        if (sample(&now) == ESP_OK) {
            portENTER_CRITICAL(&s_lock);
            s_status = now;
            s_status_valid = true;
            portEXIT_CRITICAL(&s_lock);

            const bool low = now.supply_mv < V5_LOW_WARN_MV;
            const bool changed = first || now.supply != last_supply ||
                                 now.overcurrent != last_overcurrent || low != last_low;
            if (changed || seconds_since_report >= POWER_MONITOR_REPORT_S) {
                ESP_LOGI(TAG, "5V rail %u mV, CC1 %u mV, CC2 %u mV, supply: %s",
                         (unsigned)now.supply_mv, (unsigned)now.cc1_mv, (unsigned)now.cc2_mv,
                         usb_cc_name(now.supply));
                seconds_since_report = 0;
            }
            if (changed) {
                if (now.supply == USB_CC_DEFAULT) {
                    ESP_LOGW(TAG, "Supply advertises only 500/900 mA; the radar needs a 5 V / 1.5 A charger");
                }
                if (now.overcurrent) {
                    ESP_LOGW(TAG, "U2 reports overcurrent: servo, matrix or short circuit on 5V");
                }
                if (low) {
                    ESP_LOGW(TAG, "5V rail below %u mV", (unsigned)V5_LOW_WARN_MV);
                }
            }
            last_supply = now.supply;
            last_overcurrent = now.overcurrent;
            last_low = low;
            first = false;
        } else {
            ESP_LOGW(TAG, "ADC read failed");
        }
        seconds_since_report++;
        vTaskDelay(pdMS_TO_TICKS(POWER_MONITOR_PERIOD_MS));
    }
}

static esp_err_t channel_for(int gpio, adc_channel_t *channel)
{
    adc_unit_t unit = ADC_UNIT_1;
    esp_err_t err = adc_oneshot_io_to_channel(gpio, &unit, channel);
    if (err != ESP_OK) {
        return err;
    }
    return unit == ADC_UNIT_1 ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}

esp_err_t power_monitor_start(void)
{
    esp_err_t err = channel_for(GPIO_V5_SENSE, &s_ch_v5);
    if (err == ESP_OK) {
        err = channel_for(GPIO_CC1_SENSE, &s_ch_cc1);
    }
    if (err == ESP_OK) {
        err = channel_for(GPIO_CC2_SENSE, &s_ch_cc2);
    }
    if (err != ESP_OK) {
        return err;
    }

    const adc_oneshot_unit_init_cfg_t unit_cfg = {.unit_id = ADC_UNIT_1};
    err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }
    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    const adc_channel_t channels[] = {s_ch_v5, s_ch_cc1, s_ch_cc2};
    for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        err = adc_oneshot_config_channel(s_adc, channels[i], &chan_cfg);
        if (err != ESP_OK) {
            return err;
        }
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        s_cali = NULL;
    }
#endif
    if (s_cali == NULL) {
        ESP_LOGW(TAG, "ADC calibration unavailable; readings are approximate");
    }

    /* U2 FLG is open drain, pulled up by R26 on the board; the internal pull-up only helps. */
    const gpio_config_t fault_cfg = {
        .pin_bit_mask = 1ULL << GPIO_POWER_FAULT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&fault_cfg);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(power_monitor_task, "power_monitor", RADAR_POWER_MONITOR_STACK_SIZE,
                    NULL, RADAR_POWER_MONITOR_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool power_monitor_get(power_status_t *status)
{
    if (status == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    const bool valid = s_status_valid;
    if (valid) {
        *status = s_status;
    }
    portEXIT_CRITICAL(&s_lock);
    return valid;
}
