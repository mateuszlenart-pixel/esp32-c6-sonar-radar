#pragma once

#include "esp_err.h"

#include "logic.h"

esp_err_t oled_init(void);
esp_err_t oled_show_status(const radar_status_t *status);
void oled_deinit(void);
