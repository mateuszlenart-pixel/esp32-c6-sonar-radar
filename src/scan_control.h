#pragma once

#include <stdbool.h>

int radar_scan_advance(int angle_deg,
                       int *direction,
                       bool target_locked,
                       int min_angle_deg,
                       int max_angle_deg,
                       int step_deg);
