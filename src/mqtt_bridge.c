#include "mqtt_bridge.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "logic.h"
#include "app_config.h"

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#endif

#ifndef RADAR_MQTT_URI
#define RADAR_MQTT_URI ""
#endif
#ifndef RADAR_MQTT_USERNAME
#define RADAR_MQTT_USERNAME ""
#endif
#ifndef RADAR_MQTT_PASSWORD
#define RADAR_MQTT_PASSWORD ""
#endif
#ifndef RADAR_MQTT_BASE_TOPIC
#define RADAR_MQTT_BASE_TOPIC "esp32-c6-sonar-radar"
#endif

#define MQTT_PUBLISH_PERIOD_MS 500U
#define MQTT_PUBLISH_TASK_STACK_SIZE 4096
#define MQTT_PUBLISH_TASK_PRIORITY 2

static const char *TAG = "MQTT_BRIDGE";
static esp_mqtt_client_handle_t s_client;
static bool s_publish_task_started;
static char s_device_id[18];
static char s_state_topic[96];
static char s_availability_topic[96];
static char s_scan_topic[96];
static char s_patrol_command_topic[96];
static char s_buzzer_command_topic[96];

static void publish_discovery(const char *component,
                              const char *object_id,
                              const char *name,
                              const char *device_class,
                              const char *unit,
                              const char *value_template,
                              const char *command_topic,
                              const char *state_on_template)
{
    char topic[160];
    char payload[640];
    const int topic_length = snprintf(topic, sizeof(topic),
                                      "homeassistant/%s/%s/config", component, object_id);
    if (topic_length < 0 || (size_t)topic_length >= sizeof(topic)) {
        return;
    }

    int payload_length = snprintf(
        payload, sizeof(payload),
        "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
        "\"state_topic\":\"%s\",\"value_template\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
        "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Sonar Radar\","
        "\"manufacturer\":\"Mateusz\",\"model\":\"ESP32-C6 Sonar Radar\"}",
        name, s_device_id, object_id, s_state_topic, value_template,
        s_availability_topic, s_device_id);
    if (payload_length < 0 || (size_t)payload_length >= sizeof(payload)) {
        return;
    }

    if (device_class != NULL) {
        payload_length += snprintf(payload + payload_length,
                                   sizeof(payload) - (size_t)payload_length,
                                   ",\"device_class\":\"%s\"", device_class);
    }
    if (unit != NULL) {
        payload_length += snprintf(payload + payload_length,
                                   sizeof(payload) - (size_t)payload_length,
                                   ",\"unit_of_measurement\":\"%s\"", unit);
    }
    if (command_topic != NULL && state_on_template != NULL) {
        payload_length += snprintf(payload + payload_length,
                                   sizeof(payload) - (size_t)payload_length,
                                   ",\"command_topic\":\"%s\","
                                   "\"state_on\":\"ON\",\"state_off\":\"OFF\","
                                   "\"command_on\":\"ON\",\"command_off\":\"OFF\","
                                   "\"state_value_template\":\"%s\"",
                                   command_topic, state_on_template);
    }
    if (payload_length < 0 || (size_t)payload_length + 2U >= sizeof(payload)) {
        return;
    }
    payload[payload_length++] = '}';
    payload[payload_length] = '\0';
    (void)esp_mqtt_client_publish(s_client, topic, payload, payload_length, 1, 1);
}

static void publish_discovery_configs(void)
{
    publish_discovery("sensor", "radar_distance", "Odległość celu", NULL,
                      "cm", "{{ value_json.distance_cm if value_json.distance_valid else '' }}", NULL, NULL);
    publish_discovery("sensor", "radar_angle", "Kąt celu", NULL,
                      "°", "{{ value_json.angle_deg }}", NULL, NULL);
    publish_discovery("sensor", "radar_scan_angle", "Kąt skanowania", NULL,
                      "°", "{{ value_json.scan_angle_deg }}", NULL, NULL);
    publish_discovery("binary_sensor", "radar_target", "Wykryto cel", "occupancy",
                      NULL, "{{ 'ON' if value_json.target_detected else 'OFF' }}", NULL, NULL);
    publish_discovery("binary_sensor", "radar_critical", "Alarm krytyczny", "safety",
                      NULL, "{{ 'ON' if value_json.critical else 'OFF' }}", NULL, NULL);
    publish_discovery("sensor", "radar_state", "Stan radaru", NULL,
                      NULL, "{{ value_json.state }}", NULL, NULL);
    publish_discovery("switch", "radar_patrol", "Patrol radaru", NULL,
                      NULL, "{{ 'ON' if value_json.patrol_enabled else 'OFF' }}",
                      s_patrol_command_topic, "{{ 'ON' if value_json.patrol_enabled else 'OFF' }}");
    publish_discovery("switch", "radar_buzzer", "Buzzer radaru", NULL,
                      NULL, "{{ 'ON' if value_json.buzzer_enabled else 'OFF' }}",
                      s_buzzer_command_topic, "{{ 'ON' if value_json.buzzer_enabled else 'OFF' }}");
}

static void publish_status(void)
{
    radar_status_t status;
    if (s_client == NULL || !logic_get_status(&status)) {
        return;
    }

    const char *state = "patrol";
    switch (status.state) {
    case RADAR_STATE_TARGET_LOCKED:
        state = "target_locked";
        break;
    case RADAR_STATE_CRITICAL_ALERT:
        state = "critical_alert";
        break;
    case RADAR_STATE_TARGET_LOST:
        state = "target_lost";
        break;
    case RADAR_STATE_PATROL:
    default:
        break;
    }

    const bool target_detected = status.state == RADAR_STATE_TARGET_LOCKED ||
                                 status.state == RADAR_STATE_CRITICAL_ALERT;
    const bool critical = status.state == RADAR_STATE_CRITICAL_ALERT;
    radar_thresholds_t thresholds = {
        .target_distance_cm = RADAR_TARGET_DISTANCE_CM,
        .critical_distance_cm = RADAR_CRITICAL_DISTANCE_CM,
    };
    (void)logic_get_thresholds(&thresholds);
    char payload[384];
    const int length = snprintf(payload, sizeof(payload),
                                "{\"state\":\"%s\",\"target_detected\":%s,"
                                "\"critical\":%s,\"distance_cm\":%lu,"
                                "\"angle_deg\":%d,\"scan_angle_deg\":%d,"
                                "\"distance_valid\":%s,\"patrol_enabled\":%s,"
                                "\"buzzer_enabled\":%s,\"target_threshold_cm\":%lu,"
                                "\"critical_threshold_cm\":%lu,\"timestamp_ms\":%lu}",
                                state, target_detected ? "true" : "false",
                                critical ? "true" : "false",
                                (unsigned long)status.distance_cm,
                                status.angle_deg, status.angle_deg,
                                status.distance_valid ? "true" : "false",
                                logic_is_patrol_enabled() ? "true" : "false",
                                logic_is_buzzer_enabled() ? "true" : "false",
                                (unsigned long)thresholds.target_distance_cm,
                                (unsigned long)thresholds.critical_distance_cm,
                                (unsigned long)status.timestamp_ms);
    if (length > 0 && (size_t)length < sizeof(payload)) {
        (void)esp_mqtt_client_publish(s_client, s_state_topic, payload, length, 0, 0);
    }
}

void mqtt_bridge_publish_scan(int angle_deg, bool distance_valid,
                              uint32_t distance_cm, uint32_t timestamp_ms)
{
    if (s_client == NULL) {
        return;
    }
    char payload[128];
    const int length = snprintf(payload, sizeof(payload),
        "{\"angle_deg\":%d,\"distance_cm\":%lu,\"valid\":%s,\"timestamp_ms\":%lu}",
        angle_deg, (unsigned long)distance_cm, distance_valid ? "true" : "false",
        (unsigned long)timestamp_ms);
    if (length > 0 && (size_t)length < sizeof(payload)) {
        (void)esp_mqtt_client_publish(s_client, s_scan_topic, payload, length, 0, 0);
    }
}

static void mqtt_publish_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        publish_status();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MQTT_PUBLISH_PERIOD_MS));
    }
}

static void apply_command(const char *topic, int topic_length, const char *data, int data_length)
{
    const bool enabled = data_length == 2 && strncmp(data, "ON", 2) == 0;
    const bool disabled = data_length == 3 && strncmp(data, "OFF", 3) == 0;
    if (!enabled && !disabled) {
        return;
    }
    if ((size_t)topic_length == strlen(s_patrol_command_topic) &&
        strncmp(topic, s_patrol_command_topic, (size_t)topic_length) == 0) {
        logic_set_patrol_enabled(enabled);
    } else if ((size_t)topic_length == strlen(s_buzzer_command_topic) &&
               strncmp(topic, s_buzzer_command_topic, (size_t)topic_length) == 0) {
        logic_set_buzzer_enabled(enabled);
    }
}

static void mqtt_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_base;
    esp_mqtt_event_handle_t event = event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Connected to MQTT broker");
        (void)esp_mqtt_client_subscribe(s_client, s_patrol_command_topic, 1);
        (void)esp_mqtt_client_subscribe(s_client, s_buzzer_command_topic, 1);
        (void)esp_mqtt_client_publish(s_client, s_availability_topic, "online", 6, 1, 1);
        publish_discovery_configs();
        if (!s_publish_task_started) {
            s_publish_task_started = xTaskCreate(mqtt_publish_task,
                                                 "mqtt_publish_task",
                                                 MQTT_PUBLISH_TASK_STACK_SIZE,
                                                 NULL,
                                                 MQTT_PUBLISH_TASK_PRIORITY,
                                                 NULL) == pdPASS;
        }
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from MQTT broker; reconnect will be retried");
    } else if (event_id == MQTT_EVENT_DATA) {
        apply_command(event->topic, event->topic_len, event->data, event->data_len);
    } else if (event_id == MQTT_EVENT_ERROR) {
        ESP_LOGW(TAG, "MQTT connection reported an error");
    }
}

esp_err_t mqtt_bridge_start(void)
{
    if (s_client != NULL) {
        return ESP_OK;
    }
    if (RADAR_MQTT_URI[0] == '\0' || strstr(RADAR_MQTT_URI, "YOUR_MQTT_BROKER") != NULL) {
        ESP_LOGI(TAG, "MQTT publishing is disabled until local broker settings are configured");
        return ESP_OK;
    }

    uint8_t mac[6] = {0};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_STA), TAG, "Could not read device ID");
    snprintf(s_device_id, sizeof(s_device_id), "radar_%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(s_state_topic, sizeof(s_state_topic), "%s/%s/state", RADAR_MQTT_BASE_TOPIC, s_device_id);
    snprintf(s_availability_topic, sizeof(s_availability_topic), "%s/%s/availability",
             RADAR_MQTT_BASE_TOPIC, s_device_id);
    snprintf(s_scan_topic, sizeof(s_scan_topic), "%s/%s/scan", RADAR_MQTT_BASE_TOPIC, s_device_id);
    snprintf(s_patrol_command_topic, sizeof(s_patrol_command_topic), "%s/%s/set/patrol",
             RADAR_MQTT_BASE_TOPIC, s_device_id);
    snprintf(s_buzzer_command_topic, sizeof(s_buzzer_command_topic), "%s/%s/set/buzzer",
             RADAR_MQTT_BASE_TOPIC, s_device_id);

    const esp_mqtt_client_config_t config = {
        .broker.address.uri = RADAR_MQTT_URI,
        .credentials.username = RADAR_MQTT_USERNAME,
        .credentials.authentication.password = RADAR_MQTT_PASSWORD,
        .session.last_will.topic = s_availability_topic,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_mqtt_client_register_event(s_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }
    err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }
    return ESP_OK;
}
