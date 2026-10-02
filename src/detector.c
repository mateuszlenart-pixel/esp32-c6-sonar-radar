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
            detector->state = RADAR_STATE_TARGET_LOST;
            detector->state_changed_ms = now_ms;
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

    radar_state_t next_state;
    if (detector->critical_active) {
        next_state = RADAR_STATE_CRITICAL_ALERT;
    } else if (detector->target_active) {
        next_state = RADAR_STATE_TARGET_LOCKED;
    } else if (detector->state == RADAR_STATE_TARGET_LOST &&
               (uint32_t)(now_ms - detector->state_changed_ms) <
                   RADAR_TARGET_LOST_DISPLAY_MS) {
        next_state = RADAR_STATE_TARGET_LOST;
    } else {
        next_state = RADAR_STATE_PATROL;
    }

    if (next_state != detector->state) {
        detector->state = next_state;
        detector->state_changed_ms = now_ms;
    }

    return events;
}
