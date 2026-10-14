#pragma once

#include "esp_err.h"

#include "logic.h"

esp_err_t matrix_init(void);
esp_err_t matrix_render_status(const radar_status_t *status, uint32_t now_ms);
esp_err_t matrix_set_power(bool on);
// Pauses LED scanning during a sonar ping (no-op if the matrix is not initialised).
void matrix_quiet(bool quiet);
