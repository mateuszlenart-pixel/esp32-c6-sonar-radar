#pragma once

#include "esp_err.h"

/* Starts the task that watches the ON/standby push button (GPIO_POWER_BUTTON). */
esp_err_t power_button_start(void);
