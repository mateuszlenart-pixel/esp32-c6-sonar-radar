#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "logic.h"

esp_err_t buzzer_init(void);
esp_err_t buzzer_update(const radar_status_t *status, uint32_t now_ms);
esp_err_t buzzer_silence(void);
void buzzer_deinit(void);
