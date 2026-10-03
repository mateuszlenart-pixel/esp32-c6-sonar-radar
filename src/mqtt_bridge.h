#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t mqtt_bridge_start(void);
void mqtt_bridge_publish_scan(int angle_deg, bool distance_valid,
                              uint32_t distance_cm, uint32_t timestamp_ms);
