#include "detector.h"

#include <stddef.h>

#include "app_config.h"

void radar_detector_init(radar_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    *detector = (radar_detector_t){0};
}

radar_detector_events_t radar_detector_update(radar_detector_t *detector,
                                             bool measurement_valid,
                                             uint32_t distance_cm,
                                             uint32_t now_ms)
{
    if (detector == NULL) {
        return RADAR_EVENT_NONE;
    }

    radar_detector_events_t events = RADAR_EVENT_NONE;

    if (!measurement_valid) {
        detector->target_confirm_count = 0;
        detector->critical_confirm_count = 0;
        detector->critical_clear_count = 0;
    }

    if (!detector->target_active) {
        if (measurement_valid && distance_cm <= RADAR_TARGET_DISTANCE_CM) {
            if (detector->target_confirm_count < RADAR_TARGET_CONFIRM_COUNT) {
                detector->target_confirm_count++;
            }
            if (detector->target_confirm_count >= RADAR_TARGET_CONFIRM_COUNT) {
                detector->target_active = true;
                detector->last_target_seen_ms = now_ms;
                detector->target_confirm_count = 0;
                events |= RADAR_EVENT_TARGET_ACQUIRED;
            }
        } else {
            detector->target_confirm_count = 0;
        }
    }

    if (detector->target_active) {
        if (measurement_valid && distance_cm <= RADAR_TARGET_RELEASE_DISTANCE_CM) {
            detector->last_target_seen_ms = now_ms;
        } else if ((uint32_t)(now_ms - detector->last_target_seen_ms) >=
                   RADAR_TARGET_LOST_TIMEOUT_MS) {
            detector->target_active = false;
            detector->target_confirm_count = 0;
            detector->critical_confirm_count = 0;
            detector->critical_clear_count = 0;
            events |= RADAR_EVENT_TARGET_LOST;
            if (detector->critical_active) {
                detector->critical_active = false;
                events |= RADAR_EVENT_CRITICAL_CLEARED;
            }
            return events;
        }

        if (measurement_valid && distance_cm <= RADAR_CRITICAL_DISTANCE_CM) {
            detector->critical_clear_count = 0;
            if (!detector->critical_active) {
                if (detector->critical_confirm_count < RADAR_CRITICAL_CONFIRM_COUNT) {
                    detector->critical_confirm_count++;
                }
                if (detector->critical_confirm_count >= RADAR_CRITICAL_CONFIRM_COUNT) {
                    detector->critical_active = true;
                    detector->critical_confirm_count = 0;
                    events |= RADAR_EVENT_CRITICAL_ENTERED;
                }
            }
        } else if (measurement_valid && distance_cm > RADAR_CRITICAL_RELEASE_DISTANCE_CM) {
            detector->critical_confirm_count = 0;
            if (detector->critical_active) {
                if (detector->critical_clear_count < RADAR_CRITICAL_CLEAR_COUNT) {
                    detector->critical_clear_count++;
                }
                if (detector->critical_clear_count >= RADAR_CRITICAL_CLEAR_COUNT) {
                    detector->critical_active = false;
                    detector->critical_clear_count = 0;
                    events |= RADAR_EVENT_CRITICAL_CLEARED;
                }
            }
        } else if (measurement_valid) {
            detector->critical_confirm_count = 0;
            detector->critical_clear_count = 0;
        }
    }

    return events;
}
