#pragma once

#include "esp_err.h"

#include "logic.h"

esp_err_t matrix_init(void);
esp_err_t matrix_render_status(const radar_status_t *status, uint32_t now_ms);
