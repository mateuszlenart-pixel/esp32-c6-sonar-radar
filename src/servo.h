#pragma once

#include "esp_err.h"

esp_err_t servo_init(void);
esp_err_t servo_set_angle(int angle_deg);
esp_err_t servo_release(void);
