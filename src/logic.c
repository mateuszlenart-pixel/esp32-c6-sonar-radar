#include "logic.h"

#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_config.h"
#include "mqtt_bridge.h"

#define RADAR_TARGET_LOCKED_BIT (1U << 0)
#define RADAR_PATROL_ENABLED_BIT (1U << 1)
#define RADAR_BUZZER_ENABLED_BIT (1U << 2)
#define RADAR_POWER_ENABLED_BIT (1U << 3)

typedef struct {
    int angle_deg;
    uint32_t distance_cm;
    uint32_t timestamp_ms;
    bool distance_valid;
} radar_measurement_t;

static const char *TAG = "LOGIC";
static QueueHandle_t s_measurement_queue;
static QueueHandle_t s_status_queue;
static QueueHandle_t s_threshold_queue;
static EventGroupHandle_t s_control_events;

static void logic_task(void *arg)
{
    (void)arg;

    radar_detector_t detector;
    radar_detector_init(&detector);
    radar_thresholds_t thresholds;
    radar_measurement_t measurement;

    while (xQueueReceive(s_measurement_queue, &measurement, portMAX_DELAY) == pdTRUE) {
        if (!logic_is_power_enabled()) {
            continue; /* standby: drop measurements still in flight */
        }
        if (xQueuePeek(s_threshold_queue, &thresholds, 0) == pdTRUE) {
            radar_detector_set_distance_thresholds(&detector,
                thresholds.target_distance_cm, thresholds.critical_distance_cm);
        }
        const radar_detector_events_t events = radar_detector_update(
            &detector,
            measurement.distance_valid,
            measurement.distance_cm,
            measurement.timestamp_ms);

        if (detector.target_active) {
            xEventGroupSetBits(s_control_events, RADAR_TARGET_LOCKED_BIT);
        } else {
            xEventGroupClearBits(s_control_events, RADAR_TARGET_LOCKED_BIT);
        }

        const radar_status_t status = {
            .state = detector.state,
            .angle_deg = measurement.angle_deg,
            .distance_cm = measurement.distance_cm,
            .timestamp_ms = measurement.timestamp_ms,
            .distance_valid = measurement.distance_valid,
        };
        xQueueOverwrite(s_status_queue, &status);

        if (events & RADAR_EVENT_TARGET_ACQUIRED) {
            ESP_LOGI(TAG, "Target detected at %d deg, %lu cm",
                     measurement.angle_deg, (unsigned long)measurement.distance_cm);
        }
        if (events & RADAR_EVENT_CRITICAL_ENTERED) {
            ESP_LOGW(TAG, "CRITICAL: target at %d deg, %lu cm",
                     measurement.angle_deg, (unsigned long)measurement.distance_cm);
        }
        if (events & RADAR_EVENT_CRITICAL_CLEARED) {
            ESP_LOGI(TAG, "Critical distance alert cleared");
        }
        if (events & RADAR_EVENT_TARGET_LOST) {
            ESP_LOGI(TAG, "Target lost");
        }
    }
}

esp_err_t logic_init(void)
{
    if (s_measurement_queue != NULL || s_status_queue != NULL ||
        s_threshold_queue != NULL || s_control_events != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_measurement_queue = xQueueCreate(RADAR_MEASUREMENT_QUEUE_LENGTH,
                                       sizeof(radar_measurement_t));
    s_status_queue = xQueueCreate(1, sizeof(radar_status_t));
    s_threshold_queue = xQueueCreate(1, sizeof(radar_thresholds_t));
    s_control_events = xEventGroupCreate();

    if (s_measurement_queue == NULL || s_status_queue == NULL ||
        s_threshold_queue == NULL || s_control_events == NULL) {
        if (s_measurement_queue != NULL) {
            vQueueDelete(s_measurement_queue);
            s_measurement_queue = NULL;
        }
        if (s_status_queue != NULL) {
            vQueueDelete(s_status_queue);
            s_status_queue = NULL;
        }
        if (s_threshold_queue != NULL) {
            vQueueDelete(s_threshold_queue);
            s_threshold_queue = NULL;
        }
        if (s_control_events != NULL) {
            vEventGroupDelete(s_control_events);
            s_control_events = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    const radar_status_t initial_status = {
        .state = RADAR_STATE_PATROL,
        .angle_deg = RADAR_MIN_ANGLE_DEG,
        .distance_valid = false,
    };
    xQueueOverwrite(s_status_queue, &initial_status);
    const radar_thresholds_t initial_thresholds = {
        .target_distance_cm = RADAR_TARGET_DISTANCE_CM,
        .critical_distance_cm = RADAR_CRITICAL_DISTANCE_CM,
    };
    xQueueOverwrite(s_threshold_queue, &initial_thresholds);
    xEventGroupSetBits(s_control_events,
                       RADAR_PATROL_ENABLED_BIT | RADAR_BUZZER_ENABLED_BIT |
                       RADAR_POWER_ENABLED_BIT);
    return ESP_OK;
}

esp_err_t logic_start(void)
{
    if (s_measurement_queue == NULL || s_status_queue == NULL ||
        s_threshold_queue == NULL || s_control_events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xTaskCreate(logic_task,
                    "logic_task",
                    RADAR_LOGIC_TASK_STACK_SIZE,
                    NULL,
                    RADAR_LOGIC_TASK_PRIORITY,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create logic task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool logic_submit_measurement(int angle_deg,
                              bool distance_valid,
                              uint32_t distance_cm,
                              uint32_t timestamp_ms)
{
    if (s_measurement_queue == NULL) {
        return false;
    }

    const radar_measurement_t measurement = {
        .angle_deg = angle_deg,
        .distance_cm = distance_cm,
        .timestamp_ms = timestamp_ms,
        .distance_valid = distance_valid,
    };
    const bool queued = xQueueSend(s_measurement_queue, &measurement, 0) == pdTRUE;
    if (queued) {
        mqtt_bridge_publish_scan(angle_deg, distance_valid, distance_cm, timestamp_ms);
    }
    return queued;
}

bool logic_is_target_locked(void)
{
    return s_control_events != NULL &&
           (xEventGroupGetBits(s_control_events) & RADAR_TARGET_LOCKED_BIT) != 0;
}

bool logic_get_status(radar_status_t *status)
{
    return status != NULL && s_status_queue != NULL &&
           xQueuePeek(s_status_queue, status, 0) == pdTRUE;
}

void logic_set_power_enabled(bool enabled)
{
    if (s_control_events == NULL) {
        return;
    }
    if (enabled) {
        xEventGroupSetBits(s_control_events, RADAR_POWER_ENABLED_BIT);
        return;
    }
    xEventGroupClearBits(s_control_events,
                         RADAR_POWER_ENABLED_BIT | RADAR_TARGET_LOCKED_BIT);
    if (s_measurement_queue != NULL) {
        (void)xQueueReset(s_measurement_queue);
    }
    if (s_status_queue != NULL) {
        radar_status_t status;
        if (xQueuePeek(s_status_queue, &status, 0) != pdTRUE) {
            status.angle_deg = RADAR_MIN_ANGLE_DEG;
        }
        status.state = RADAR_STATE_PATROL;
        status.distance_cm = 0;
        status.distance_valid = false;
        (void)xQueueOverwrite(s_status_queue, &status);
    }
}

bool logic_is_power_enabled(void)
{
    return s_control_events != NULL &&
           (xEventGroupGetBits(s_control_events) & RADAR_POWER_ENABLED_BIT) != 0;
}

void logic_set_patrol_enabled(bool enabled)
{
    if (s_control_events == NULL) {
        return;
    }
    if (enabled) {
        xEventGroupSetBits(s_control_events, RADAR_PATROL_ENABLED_BIT);
    } else {
        xEventGroupClearBits(s_control_events, RADAR_PATROL_ENABLED_BIT);
    }
}

bool logic_is_patrol_enabled(void)
{
    return s_control_events != NULL &&
           (xEventGroupGetBits(s_control_events) & RADAR_PATROL_ENABLED_BIT) != 0;
}

void logic_set_buzzer_enabled(bool enabled)
{
    if (s_control_events == NULL) {
        return;
    }
    if (enabled) {
        xEventGroupSetBits(s_control_events, RADAR_BUZZER_ENABLED_BIT);
    } else {
        xEventGroupClearBits(s_control_events, RADAR_BUZZER_ENABLED_BIT);
    }
}

bool logic_is_buzzer_enabled(void)
{
    return s_control_events != NULL &&
           (xEventGroupGetBits(s_control_events) & RADAR_BUZZER_ENABLED_BIT) != 0;
}

bool logic_get_thresholds(radar_thresholds_t *thresholds)
{
    return thresholds != NULL && s_threshold_queue != NULL &&
           xQueuePeek(s_threshold_queue, thresholds, 0) == pdTRUE;
}

bool logic_set_thresholds(uint32_t target_distance_cm, uint32_t critical_distance_cm)
{
    if (s_threshold_queue == NULL || critical_distance_cm >= target_distance_cm ||
        critical_distance_cm < 5U || critical_distance_cm > 300U ||
        target_distance_cm < 10U || target_distance_cm > 400U) {
        return false;
    }
    const radar_thresholds_t thresholds = {
        .target_distance_cm = target_distance_cm,
        .critical_distance_cm = critical_distance_cm,
    };
    return xQueueOverwrite(s_threshold_queue, &thresholds) == pdPASS;
}
