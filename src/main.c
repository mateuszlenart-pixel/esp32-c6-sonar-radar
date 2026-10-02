#include "esp_app_desc.h"
#include "esp_log.h"

#include "app_config.h"
#include "logic.h"
#include "radar.h"
#include "sonar.h"
#include "sonar_task.h"
#include "servo.h"

static const char *TAG = "RADAR";

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();

    ESP_LOGI(TAG, "Sonar radar firmware starting");
    ESP_LOGI(TAG, "Project: %s, version: %s", app->project_name, app->version);
    ESP_LOGI(TAG, "Configured scan range: %d-%d degrees",
             RADAR_MIN_ANGLE_DEG, RADAR_MAX_ANGLE_DEG);

    ESP_ERROR_CHECK(servo_init());
    ESP_ERROR_CHECK(sonar_init());
    ESP_ERROR_CHECK(logic_init());
    ESP_ERROR_CHECK(logic_start());
    ESP_ERROR_CHECK(sonar_task_start());
    radar_start();
}
