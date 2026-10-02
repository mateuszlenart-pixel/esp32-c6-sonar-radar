#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RADAR_EVENT_NONE = 0,
    RADAR_EVENT_TARGET_ACQUIRED = 1U << 0,
    RADAR_EVENT_TARGET_LOST = 1U << 1,
    RADAR_EVENT_CRITICAL_ENTERED = 1U << 2,
    RADAR_EVENT_CRITICAL_CLEARED = 1U << 3,
} radar_detector_events_t;

typedef enum {
    RADAR_STATE_PATROL = 0,
    RADAR_STATE_TARGET_LOCKED,
    RADAR_STATE_CRITICAL_ALERT,
    RADAR_STATE_TARGET_LOST,
} radar_state_t;

typedef struct {
    uint8_t target_confirm_count;
    uint8_t critical_confirm_count;
    uint8_t critical_clear_count;
    bool target_active;
    bool critical_active;
    uint32_t last_target_seen_ms;
    uint32_t state_changed_ms;
    radar_state_t state;
} radar_detector_t;

void radar_detector_init(radar_detector_t *detector);
radar_detector_events_t radar_detector_update(radar_detector_t *detector,
                                             bool measurement_valid,
                                             uint32_t distance_cm,
                                             uint32_t now_ms);
