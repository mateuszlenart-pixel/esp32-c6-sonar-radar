#include "network.h"

#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#else
#define RADAR_WIFI_SSID ""
#define RADAR_WIFI_PASSWORD ""
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_RECONNECT_BIT BIT1

static const char *TAG = "NETWORK";
static EventGroupHandle_t s_wifi_events;
static bool s_started;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_events, WIFI_RECONNECT_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_events, WIFI_RECONNECT_BIT);
        ESP_LOGW(TAG, "Wi-Fi disconnected; retry scheduled");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Connected; IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupClearBits(s_wifi_events, WIFI_RECONNECT_BIT);
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void network_task(void *arg)
{
    (void)arg;

    while (true) {
        xEventGroupWaitBits(s_wifi_events,
                            WIFI_RECONNECT_BIT,
                            pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(RADAR_WIFI_RETRY_DELAY_MS));

        if ((xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) == 0) {
            const esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Wi-Fi connect request failed: %s", esp_err_to_name(err));
            }
        }
    }
}

static esp_err_t init_wifi(void)
{
    const size_t ssid_length = strlen(RADAR_WIFI_SSID);
    const size_t password_length = strlen(RADAR_WIFI_PASSWORD);
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, RADAR_WIFI_SSID, ssid_length);
    memcpy(wifi_config.sta.password, RADAR_WIFI_PASSWORD, password_length);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        return err;
    }

    return esp_wifi_start();
}

esp_err_t network_start(void)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (RADAR_WIFI_SSID[0] == '\0') {
        ESP_LOGI(TAG, "Wi-Fi disabled; no local credentials file is configured");
        return ESP_OK;
    }
    const size_t ssid_length = strlen(RADAR_WIFI_SSID);
    const size_t password_length = strlen(RADAR_WIFI_PASSWORD);
    if (ssid_length > sizeof(((wifi_config_t *)0)->sta.ssid) ||
        password_length > sizeof(((wifi_config_t *)0)->sta.password)) {
        ESP_LOGE(TAG, "Wi-Fi credentials exceed ESP-IDF limits");
        return ESP_ERR_INVALID_ARG;
    }

    s_wifi_events = xEventGroupCreate();
    if (s_wifi_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t err = init_wifi();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(network_task,
                    "network_task",
                    RADAR_NETWORK_TASK_STACK_SIZE,
                    NULL,
                    RADAR_NETWORK_TASK_PRIORITY,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create Wi-Fi reconnect task");
        (void)esp_wifi_stop();
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    return ESP_OK;
}

bool network_is_connected(void)
{
    return s_wifi_events != NULL &&
           (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) != 0;
}
