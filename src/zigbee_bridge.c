#include "zigbee_bridge.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "logic.h"
#include "app_config.h"

#define RADAR_ENDPOINT 1
#define RADAR_CRITICAL_ENDPOINT 2
#define RADAR_ANGLE_ENDPOINT 3
#define RADAR_DISTANCE_ENDPOINT 4
#define RADAR_PATROL_ENDPOINT 5
#define RADAR_BUZZER_ENDPOINT 6
#define RADAR_TARGET_THRESHOLD_ENDPOINT 7
#define RADAR_CRITICAL_THRESHOLD_ENDPOINT 8
#define RADAR_POWER_ENDPOINT 9
#define RADAR_ZIGBEE_TASK_STACK 6144
#define RADAR_ZIGBEE_TASK_PRIORITY 5
#define RADAR_ZIGBEE_RETRY_DELAY_MS 5000

static const char *TAG = "ZIGBEE_BRIDGE";
static bool s_started;
static TimerHandle_t s_retry_timer;

static void zigbee_retry_timer(TimerHandle_t timer)
{
    (void)timer;
    if (esp_zigbee_lock_acquire(pdMS_TO_TICKS(100))) {
        (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        esp_zigbee_lock_release();
    }
}

static bool add_endpoint(ezb_af_device_desc_t device, uint8_t endpoint_id,
                         ezb_zcl_cluster_desc_t cluster,
                         ezb_zcl_cluster_desc_t extra_cluster)
{
    const ezb_af_ep_config_t endpoint_config = {
        .ep_id = endpoint_id,
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 1,
    };
    ezb_af_ep_desc_t endpoint = ezb_af_create_endpoint_desc(&endpoint_config);
    if (endpoint == EZB_INVALID_AF_EP_DESC) {
        return false;
    }
    return ezb_af_endpoint_add_cluster_desc(endpoint, cluster) == EZB_ERR_NONE &&
           (extra_cluster == EZB_INVALID_DESC ||
            ezb_af_endpoint_add_cluster_desc(endpoint, extra_cluster) == EZB_ERR_NONE) &&
           ezb_af_device_add_endpoint_desc(device, endpoint) == EZB_ERR_NONE;
}

static ezb_af_device_desc_t create_radar_descriptor(void)
{
    ezb_af_device_desc_t device = ezb_af_create_device_desc();
    if (device == EZB_INVALID_AF_DEVICE_DESC) {
        return EZB_INVALID_AF_DEVICE_DESC;
    }

    const ezb_zcl_occupancy_sensing_cluster_server_config_t occupancy_config = {
        .occupancy = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,
        .occupancy_sensor_type = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ULTRASONIC,
        .occupancy_sensor_type_bitmap = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ULTRASONIC,
    };
    const ezb_zcl_basic_cluster_server_config_t basic_config = {
        .zcl_version = 3,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_SINGLE_PHASE_MAINS,
    };
    const ezb_zcl_analog_input_cluster_server_config_t angle_config = {
        .out_of_service = false,
        .present_value = RADAR_MIN_ANGLE_DEG,
        .status_flags = 0,
    };
    const ezb_zcl_analog_input_cluster_server_config_t distance_config = {
        .out_of_service = false,
        .present_value = 0.0f,
        .status_flags = 0,
    };

    ezb_zcl_cluster_desc_t occupancy = ezb_zcl_occupancy_sensing_create_cluster_desc(
        &occupancy_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t critical = ezb_zcl_occupancy_sensing_create_cluster_desc(
        &occupancy_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t angle = ezb_zcl_analog_input_create_cluster_desc(
        &angle_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t distance = ezb_zcl_analog_input_create_cluster_desc(
        &distance_config, EZB_ZCL_CLUSTER_SERVER);
    const ezb_zcl_on_off_cluster_server_config_t enabled_config = {.on_off = true};
    ezb_zcl_cluster_desc_t patrol = ezb_zcl_on_off_create_cluster_desc(
        &enabled_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t buzzer = ezb_zcl_on_off_create_cluster_desc(
        &enabled_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t power = ezb_zcl_on_off_create_cluster_desc(
        &enabled_config, EZB_ZCL_CLUSTER_SERVER);
    const ezb_zcl_analog_output_cluster_server_config_t target_threshold_config = {
        .out_of_service = false,
        .present_value = RADAR_TARGET_DISTANCE_CM,
        .status_flags = 0,
    };
    const ezb_zcl_analog_output_cluster_server_config_t critical_threshold_config = {
        .out_of_service = false,
        .present_value = RADAR_CRITICAL_DISTANCE_CM,
        .status_flags = 0,
    };
    ezb_zcl_cluster_desc_t target_threshold = ezb_zcl_analog_output_create_cluster_desc(
        &target_threshold_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t critical_threshold = ezb_zcl_analog_output_create_cluster_desc(
        &critical_threshold_config, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_cluster_desc_t basic = ezb_zcl_basic_create_cluster_desc(
        &basic_config, EZB_ZCL_CLUSTER_SERVER);
    static uint8_t manufacturer_name[] = {0x0e, 'M', 'a', 't', 'e', 'u', 's', 'z', ' ',
                                          'L', 'e', 'n', 'a', 'r', 't'};
    static uint8_t model_identifier[] = {0x12, 'E', 'S', 'P', '3', '2', 'C', '6', '-',
                                         'S', 'o', 'n', 'a', 'r', 'R', 'a', 'd', 'a', 'r'};
    const bool basic_attributes_added = basic != EZB_INVALID_DESC &&
        ezb_zcl_basic_cluster_desc_add_attr(basic,
            EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer_name) == EZB_ERR_NONE &&
        ezb_zcl_basic_cluster_desc_add_attr(basic,
            EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model_identifier) == EZB_ERR_NONE;
    const float target_minimum = 10.0f;
    const float target_maximum = 400.0f;
    const float critical_minimum = 5.0f;
    const float critical_maximum = 300.0f;
    const bool threshold_attributes_added =
        target_threshold != EZB_INVALID_DESC && critical_threshold != EZB_INVALID_DESC &&
        ezb_zcl_analog_output_cluster_desc_add_attr(target_threshold,
            EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, &target_minimum) == EZB_ERR_NONE &&
        ezb_zcl_analog_output_cluster_desc_add_attr(target_threshold,
            EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, &target_maximum) == EZB_ERR_NONE &&
        ezb_zcl_analog_output_cluster_desc_add_attr(critical_threshold,
            EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, &critical_minimum) == EZB_ERR_NONE &&
        ezb_zcl_analog_output_cluster_desc_add_attr(critical_threshold,
            EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, &critical_maximum) == EZB_ERR_NONE;
    if (occupancy == EZB_INVALID_DESC || angle == EZB_INVALID_DESC ||
        distance == EZB_INVALID_DESC) {
        ezb_af_free_device_desc(device);
        return EZB_INVALID_AF_DEVICE_DESC;
    }

    /* Give every measurement its own endpoint so no cluster is registered twice. */
    if (occupancy == EZB_INVALID_DESC || critical == EZB_INVALID_DESC ||
        angle == EZB_INVALID_DESC || distance == EZB_INVALID_DESC ||
        patrol == EZB_INVALID_DESC || buzzer == EZB_INVALID_DESC ||
        power == EZB_INVALID_DESC || !basic_attributes_added ||
        !threshold_attributes_added ||
        !add_endpoint(device, RADAR_ENDPOINT, occupancy, basic) ||
        !add_endpoint(device, RADAR_CRITICAL_ENDPOINT, critical, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_ANGLE_ENDPOINT, angle, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_DISTANCE_ENDPOINT, distance, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_PATROL_ENDPOINT, patrol, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_BUZZER_ENDPOINT, buzzer, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_TARGET_THRESHOLD_ENDPOINT, target_threshold, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_CRITICAL_THRESHOLD_ENDPOINT, critical_threshold, EZB_INVALID_DESC) ||
        !add_endpoint(device, RADAR_POWER_ENDPOINT, power, EZB_INVALID_DESC)) {
        ezb_af_free_device_desc(device);
        return EZB_INVALID_AF_DEVICE_DESC;
    }
    return device;
}

static void zigbee_attribute_handler(ezb_zcl_core_action_callback_id_t callback_id,
                                     void *message)
{
    if (callback_id != EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID || message == NULL) {
        return;
    }
    const ezb_zcl_set_attr_value_message_t *change = message;
    if (change->info.cluster_id == EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT &&
        change->in.attribute.id == EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID &&
        change->in.attribute.data.value != NULL &&
        (change->info.dst_ep == RADAR_TARGET_THRESHOLD_ENDPOINT ||
         change->info.dst_ep == RADAR_CRITICAL_THRESHOLD_ENDPOINT)) {
        const float value = *(const float *)change->in.attribute.data.value;
        radar_thresholds_t current;
        if (logic_get_thresholds(&current)) {
            const bool is_target = change->info.dst_ep == RADAR_TARGET_THRESHOLD_ENDPOINT;
            const bool value_in_range = is_target
                ? value >= 10.0f && value <= 400.0f
                : value >= 5.0f && value <= 300.0f;
            const uint32_t target = change->info.dst_ep == RADAR_TARGET_THRESHOLD_ENDPOINT
                ? (value_in_range ? (uint32_t)(value + 0.5f) : 0U)
                : current.target_distance_cm;
            const uint32_t critical = change->info.dst_ep == RADAR_CRITICAL_THRESHOLD_ENDPOINT
                ? (value_in_range ? (uint32_t)(value + 0.5f) : 0U)
                : current.critical_distance_cm;
            if (!value_in_range || !logic_set_thresholds(target, critical)) {
                const float previous = change->info.dst_ep == RADAR_TARGET_THRESHOLD_ENDPOINT
                    ? (float)current.target_distance_cm : (float)current.critical_distance_cm;
                (void)ezb_zcl_set_attr_value(change->info.dst_ep,
                    EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER,
                    EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE,
                    (void *)&previous, false);
                ESP_LOGW(TAG, "Rejected incompatible radar distance threshold");
            }
        }
        return;
    }
    if (change->info.cluster_id != EZB_ZCL_CLUSTER_ID_ON_OFF ||
        change->in.attribute.id != EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID ||
        change->in.attribute.data.value == NULL) {
        return;
    }
    const bool enabled = *(const uint8_t *)change->in.attribute.data.value != 0;
    if (change->info.dst_ep == RADAR_PATROL_ENDPOINT) {
        logic_set_patrol_enabled(enabled);
        ESP_LOGI(TAG, "Patrol %s from Zigbee", enabled ? "enabled" : "paused");
    } else if (change->info.dst_ep == RADAR_BUZZER_ENDPOINT) {
        logic_set_buzzer_enabled(enabled);
        ESP_LOGI(TAG, "Buzzer %s from Zigbee", enabled ? "enabled" : "muted");
    } else if (change->info.dst_ep == RADAR_POWER_ENDPOINT) {
        logic_set_power_enabled(enabled);
        ESP_LOGI(TAG, "Radar %s from Zigbee", enabled ? "ON" : "standby");
    }
}

static void zigbee_update_task(void *arg)
{
    (void)arg;
    radar_status_t status;
    while (true) {
        if (logic_get_status(&status) && esp_zigbee_lock_acquire(pdMS_TO_TICKS(100))) {
            uint8_t occupancy = logic_is_target_locked() ?
                EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_OCCUPIED :
                EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED;
            uint8_t critical = status.state == RADAR_STATE_CRITICAL_ALERT ?
                EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_OCCUPIED :
                EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED;
            float angle = (float)status.angle_deg;
            float distance = (float)status.distance_cm;
            bool distance_out_of_service = !status.distance_valid;
            uint8_t patrol = logic_is_patrol_enabled() ? 1U : 0U;
            uint8_t buzzer = logic_is_buzzer_enabled() ? 1U : 0U;
            uint8_t power = logic_is_power_enabled() ? 1U : 0U;
            radar_thresholds_t thresholds;
            if (!logic_get_thresholds(&thresholds)) {
                thresholds.target_distance_cm = RADAR_TARGET_DISTANCE_CM;
                thresholds.critical_distance_cm = RADAR_CRITICAL_DISTANCE_CM;
            }
            float target_threshold = (float)thresholds.target_distance_cm;
            float critical_threshold = (float)thresholds.critical_distance_cm;
            (void)ezb_zcl_set_attr_value(RADAR_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, EZB_ZCL_STD_MANUF_CODE,
                &occupancy, false);
            (void)ezb_zcl_set_attr_value(RADAR_CRITICAL_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, EZB_ZCL_STD_MANUF_CODE,
                &critical, false);
            (void)ezb_zcl_set_attr_value(RADAR_ANGLE_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE,
                &angle, false);
            (void)ezb_zcl_set_attr_value(RADAR_DISTANCE_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID, EZB_ZCL_STD_MANUF_CODE,
                &distance_out_of_service, false);
            if (status.distance_valid) {
                (void)ezb_zcl_set_attr_value(RADAR_DISTANCE_ENDPOINT,
                    EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER,
                    EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE,
                    &distance, false);
            }
            (void)ezb_zcl_set_attr_value(RADAR_PATROL_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE,
                &patrol, false);
            (void)ezb_zcl_set_attr_value(RADAR_BUZZER_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE,
                &buzzer, false);
            (void)ezb_zcl_set_attr_value(RADAR_POWER_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE,
                &power, false);
            (void)ezb_zcl_set_attr_value(RADAR_TARGET_THRESHOLD_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE,
                &target_threshold, false);
            (void)ezb_zcl_set_attr_value(RADAR_CRITICAL_THRESHOLD_ENDPOINT,
                EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER,
                EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE,
                &critical_threshold, false);
            esp_zigbee_lock_release();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static bool zigbee_signal_handler(const ezb_app_signal_t *signal)
{
    const ezb_app_signal_type_t type = ezb_app_signal_get_type(signal);
    bool handled = false;
    if (type == EZB_ZDO_SIGNAL_SKIP_STARTUP) {
        handled = true;
        ESP_LOGI(TAG, "Starting Zigbee commissioning");
        (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
    } else if (type == EZB_BDB_SIGNAL_DEVICE_FIRST_START ||
               type == EZB_BDB_SIGNAL_DEVICE_REBOOT ||
               type == EZB_BDB_SIGNAL_STEERING) {
        handled = true;
        const ezb_bdb_signal_simple_params_t *params = ezb_app_signal_get_params(signal);
        if (params != NULL && params->status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Zigbee connected or commissioning completed");
            ezb_nwk_set_rx_on_when_idle(true);
            if (type == EZB_BDB_SIGNAL_DEVICE_FIRST_START) {
                (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
            }
        } else {
            ESP_LOGW(TAG, "Zigbee commissioning not complete; retry scheduled");
            if (s_retry_timer != NULL) {
                (void)xTimerStart(s_retry_timer, 0);
            }
        }
    }
    return handled;
}

static void zigbee_task(void *arg)
{
    (void)arg;
    const esp_zigbee_config_t config = {
        .device_config = {
            .device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE,
            .install_code_policy = false,
            .zed_config = {
                .ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN,
                .keep_alive = 3000,
            },
        },
        .platform_config = {
            .storage_partition_name = "zb_storage",
            .radio_config.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,
        },
    };

    esp_err_t err = nvs_flash_init_partition("zb_storage");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Zigbee storage: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    s_retry_timer = xTimerCreate("zigbee_retry",
                                 pdMS_TO_TICKS(RADAR_ZIGBEE_RETRY_DELAY_MS),
                                 pdFALSE, NULL, zigbee_retry_timer);
    if (s_retry_timer == NULL) {
        ESP_LOGE(TAG, "Could not create Zigbee retry timer");
        vTaskDelete(NULL);
        return;
    }
    ezb_af_device_desc_t descriptor = create_radar_descriptor();
    if (descriptor == EZB_INVALID_AF_DEVICE_DESC) {
        ESP_LOGE(TAG, "Could not create Zigbee device endpoints");
        vTaskDelete(NULL);
        return;
    }
    if (esp_zigbee_init(&config) != ESP_OK ||
        ezb_af_device_desc_register(descriptor) != EZB_ERR_NONE ||
        ezb_app_signal_add_handler(zigbee_signal_handler) != EZB_ERR_NONE) {
        ESP_LOGE(TAG, "Could not start Zigbee stack");
        vTaskDelete(NULL);
        return;
    }
    ezb_zcl_core_action_handler_register(zigbee_attribute_handler);
    if (esp_zigbee_start(false) != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Zigbee stack");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Zigbee end-device stack started; join it from Zigbee2MQTT");
    if (xTaskCreate(zigbee_update_task, "zigbee_update", 3072, NULL,
                    RADAR_ZIGBEE_TASK_PRIORITY - 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create Zigbee update task");
    }
    (void)esp_zigbee_launch_mainloop();
    vTaskDelete(NULL);
}

esp_err_t zigbee_bridge_start(void)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskCreate(zigbee_task, "zigbee_task", RADAR_ZIGBEE_TASK_STACK,
                    NULL, RADAR_ZIGBEE_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    return ESP_OK;
}
