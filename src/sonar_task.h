#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t sonar_task_start(void);
bool sonar_task_request_measurement(int angle_deg);
