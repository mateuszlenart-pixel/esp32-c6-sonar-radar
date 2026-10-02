#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "detector.h"

typedef struct {
    radar_state_t state;
    int angle_deg;
    uint32_t distance_cm;
    uint32_t timestamp_ms;
    bool distance_valid;
} radar_status_t;

esp_err_t logic_init(void);
esp_err_t logic_start(void);
bool logic_submit_measurement(int angle_deg,
                              bool distance_valid,
                              uint32_t distance_cm,
                              uint32_t timestamp_ms);
bool logic_is_target_locked(void);
bool logic_get_status(radar_status_t *status);
