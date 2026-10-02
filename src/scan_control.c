#include "scan_control.h"

int radar_scan_advance(int angle_deg,
                       int *direction,
                       bool target_locked,
                       int min_angle_deg,
                       int max_angle_deg,
                       int step_deg)
{
    if (target_locked || direction == 0 || min_angle_deg >= max_angle_deg || step_deg <= 0 ||
        (*direction != -1 && *direction != 1)) {
        return angle_deg;
    }

    angle_deg += *direction * step_deg;
    if (angle_deg >= max_angle_deg) {
        *direction = -1;
        return max_angle_deg;
    }
    if (angle_deg <= min_angle_deg) {
        *direction = 1;
        return min_angle_deg;
    }
    return angle_deg;
}
