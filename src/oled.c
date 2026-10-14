#include "oled.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "app_config.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGE_COUNT (OLED_HEIGHT / 8)
#define OLED_I2C_TIMEOUT_MS 100

typedef struct {
    char character;
    uint8_t columns[5];
} glyph_t;

static const char *TAG = "OLED";
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_oled_device;
static uint8_t s_framebuffer[OLED_WIDTH * OLED_PAGE_COUNT];

static const glyph_t s_font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
};

static esp_err_t write_commands(const uint8_t *commands, size_t count)
{
    if (s_oled_device == NULL || commands == NULL || count == 0 || count > 32) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t transfer[33];
    transfer[0] = 0x00;
    memcpy(&transfer[1], commands, count);
    return i2c_master_transmit(s_oled_device,
                               transfer,
                               count + 1,
                               OLED_I2C_TIMEOUT_MS);
}

static const uint8_t *find_glyph(char character)
{
    for (size_t i = 0; i < sizeof(s_font) / sizeof(s_font[0]); ++i) {
        if (s_font[i].character == character) {
            return s_font[i].columns;
        }
    }
    return s_font[0].columns;
}

static void draw_text(uint8_t x, uint8_t y, const char *text)
{
    while (*text != '\0' && x + 5 < OLED_WIDTH) {
        const uint8_t *glyph = find_glyph(*text++);
        for (uint8_t column = 0; column < 5; ++column) {
            const uint8_t pixels = glyph[column];
            for (uint8_t row = 0; row < 7; ++row) {
                if ((pixels & (1U << row)) != 0 && y + row < OLED_HEIGHT) {
                    const uint16_t index = ((y + row) / 8) * OLED_WIDTH + x + column;
                    s_framebuffer[index] |= (uint8_t)(1U << ((y + row) % 8));
                }
            }
        }
        x += 6;
    }
}

static const char *state_name(radar_state_t state)
{
    switch (state) {
    case RADAR_STATE_TARGET_LOCKED:
        return "LOCKED";
    case RADAR_STATE_CRITICAL_ALERT:
        return "CRITICAL";
    case RADAR_STATE_TARGET_LOST:
        return "LOST";
    case RADAR_STATE_PATROL:
    default:
        return "PATROL";
    }
}

esp_err_t oled_init(void)
{
    if (s_i2c_bus != NULL || s_oled_device != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = OLED_I2C_PORT,
        .sda_io_num = GPIO_OLED_SDA,
        .scl_io_num = GPIO_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        return err;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDRESS,
        .scl_speed_hz = OLED_I2C_FREQUENCY_HZ,
    };
    err = i2c_master_bus_add_device(s_i2c_bus, &device_config, &s_oled_device);
    if (err != ESP_OK) {
        oled_deinit();
        return err;
    }

    const uint8_t init_commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0xAD, 0x8B, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F,
        0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF,
    };
    err = write_commands(init_commands, sizeof(init_commands));
    if (err != ESP_OK) {
        oled_deinit();
        return err;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    for (uint8_t page = 0; page < OLED_PAGE_COUNT; ++page) {
        const uint8_t page_commands[] = {
            (uint8_t)(0xB0 | page), 0x02, 0x10,
        };
        err = write_commands(page_commands, sizeof(page_commands));
        if (err != ESP_OK) {
            oled_deinit();
            return err;
        }

        uint8_t transfer[OLED_WIDTH + 1];
        transfer[0] = 0x40;
        memcpy(&transfer[1], &s_framebuffer[page * OLED_WIDTH], OLED_WIDTH);
        err = i2c_master_transmit(s_oled_device,
                                  transfer,
                                  sizeof(transfer),
                                  OLED_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            oled_deinit();
            return err;
        }
    }

    ESP_LOGI(TAG, "SH1106 OLED initialized on I2C address 0x%02X", OLED_I2C_ADDRESS);
    return ESP_OK;
}

esp_err_t oled_show_status(const radar_status_t *status)
{
    if (status == NULL || s_oled_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(s_framebuffer, 0, sizeof(s_framebuffer));

    char line[24];
    snprintf(line, sizeof(line), "RADAR: %s", state_name(status->state));
    draw_text(0, 0, line);

    snprintf(line, sizeof(line), "ANGLE:%3d DEG", status->angle_deg);
    draw_text(0, 16, line);

    if (status->distance_valid) {
        snprintf(line, sizeof(line), "DIST:%3lu CM", (unsigned long)status->distance_cm);
    } else {
        snprintf(line, sizeof(line), "DIST:--- CM");
    }
    draw_text(0, 32, line);

    if (status->state == RADAR_STATE_CRITICAL_ALERT) {
        draw_text(0, 48, "CRITICAL ALERT");
    } else if (status->state == RADAR_STATE_TARGET_LOCKED) {
        draw_text(0, 48, "TARGET LOCKED");
    } else if (status->state == RADAR_STATE_TARGET_LOST) {
        draw_text(0, 48, "RESUME PATROL");
    } else {
        draw_text(0, 48, "SCANNING AREA");
    }

    for (uint8_t page = 0; page < OLED_PAGE_COUNT; ++page) {
        const uint8_t page_commands[] = {
            (uint8_t)(0xB0 | page), 0x02, 0x10,
        };
        esp_err_t err = write_commands(page_commands, sizeof(page_commands));
        if (err != ESP_OK) {
            return err;
        }

        uint8_t transfer[OLED_WIDTH + 1];
        transfer[0] = 0x40;
        memcpy(&transfer[1], &s_framebuffer[page * OLED_WIDTH], OLED_WIDTH);
        err = i2c_master_transmit(s_oled_device,
                                  transfer,
                                  sizeof(transfer),
                                  OLED_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t oled_set_power(bool on)
{
    if (s_oled_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t command[] = {on ? 0xAF : 0xAE};
    return write_commands(command, sizeof(command));
}

void oled_deinit(void)
{
    if (s_oled_device != NULL) {
        i2c_master_bus_rm_device(s_oled_device);
        s_oled_device = NULL;
    }
    if (s_i2c_bus != NULL) {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
}
