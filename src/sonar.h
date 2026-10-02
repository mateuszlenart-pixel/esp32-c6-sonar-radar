#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t sonar_init(void);
esp_err_t sonar_measure_cm(uint32_t *distance_cm);
