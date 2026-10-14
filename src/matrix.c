#include "matrix.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "app_config.h"

#define MATRIX_SIZE 8
#define MATRIX_REGISTER_DECODE_MODE 0x09
#define MATRIX_REGISTER_INTENSITY 0x0A
#define MATRIX_REGISTER_SCAN_LIMIT 0x0B
#define MATRIX_REGISTER_SHUTDOWN 0x0C
#define MATRIX_REGISTER_DISPLAY_TEST 0x0F

static bool s_initialized;
static bool s_powered = true;
// While the sonar pings, matrix scanning is paused (shutdown): its row switching injects current
// spikes that the HC-SR04 receiver mistakes for echoes. The UI and sonar tasks share the bus.
static bool s_quiet;
static SemaphoreHandle_t s_lock;

static uint8_t shutdown_value(void)
{
    return (s_powered && !s_quiet) ? 1 : 0;
}

static void write_bit(bool level)
{
    // Short pauses let the long breadboard wires settle, so ringing is not seen as extra clocks.
    gpio_set_level(GPIO_MAX7219_CLK, 0);
    gpio_set_level(GPIO_MAX7219_DIN, level ? 1 : 0);
    esp_rom_delay_us(1);
    gpio_set_level(GPIO_MAX7219_CLK, 1);
    esp_rom_delay_us(1);
}

static void write_register(uint8_t address, uint8_t value)
{
    gpio_set_level(GPIO_MAX7219_CS, 0);
    for (int bit = 7; bit >= 0; --bit) {
        write_bit((address & (1U << bit)) != 0);
    }
    for (int bit = 7; bit >= 0; --bit) {
        write_bit((value & (1U << bit)) != 0);
    }
    gpio_set_level(GPIO_MAX7219_CLK, 0);
    gpio_set_level(GPIO_MAX7219_CS, 1);
}

static void set_pixel(uint8_t frame[MATRIX_SIZE], int x, int y)
{
    if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) {
        frame[y] |= (uint8_t)(1U << (MATRIX_SIZE - 1 - x));
    }
}

static void draw_line(uint8_t frame[MATRIX_SIZE], int x0, int y0, int x1, int y1)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        set_pixel(frame, x0, y0);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int doubled_error = 2 * error;
        if (doubled_error >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void render_patrol(uint8_t frame[MATRIX_SIZE], int angle_deg)
{
    static const uint8_t endpoints[8][2] = {
        {0, 2}, {0, 1}, {1, 0}, {2, 0},
        {5, 0}, {6, 0}, {7, 1}, {7, 2},
    };
    int index = ((angle_deg - RADAR_MIN_ANGLE_DEG) * 7) /
                (RADAR_MAX_ANGLE_DEG - RADAR_MIN_ANGLE_DEG);
    if (index < 0) {
        index = 0;
    } else if (index > 7) {
        index = 7;
    }

    draw_line(frame, 3, 7, endpoints[index][0], endpoints[index][1]);
    set_pixel(frame, 0, 3);
    set_pixel(frame, 1, 2);
    set_pixel(frame, 2, 1);
    set_pixel(frame, 3, 1);
    set_pixel(frame, 4, 1);
    set_pixel(frame, 5, 2);
    set_pixel(frame, 6, 3);
}

static void render_target(uint8_t frame[MATRIX_SIZE])
{
    for (int offset = 1; offset <= 2; ++offset) {
        set_pixel(frame, 3 - offset, 3);
        set_pixel(frame, 4 + offset, 3);
        set_pixel(frame, 3 - offset, 4);
        set_pixel(frame, 4 + offset, 4);
        set_pixel(frame, 3, 3 - offset);
        set_pixel(frame, 4, 3 - offset);
        set_pixel(frame, 3, 4 + offset);
        set_pixel(frame, 4, 4 + offset);
    }
    set_pixel(frame, 3, 3);
    set_pixel(frame, 4, 3);
    set_pixel(frame, 3, 4);
    set_pixel(frame, 4, 4);
}

static void render_alert(uint8_t frame[MATRIX_SIZE], bool visible)
{
    if (!visible) {
        return;
    }
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        set_pixel(frame, i, i);
        set_pixel(frame, MATRIX_SIZE - 1 - i, i);
    }
}

// Control registers are re-sent with every frame: a supply glitch or a corrupted transfer
// can reset the MAX7219 or switch it off, and it would otherwise stay dark until reboot.
static void write_config(void)
{
    write_register(MATRIX_REGISTER_DISPLAY_TEST, 0);
    write_register(MATRIX_REGISTER_DECODE_MODE, 0);
    write_register(MATRIX_REGISTER_SCAN_LIMIT, 7);
    write_register(MATRIX_REGISTER_INTENSITY, MAX7219_INTENSITY);
}

esp_err_t matrix_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << GPIO_MAX7219_DIN) |
                        (1ULL << GPIO_MAX7219_CLK) |
                        (1ULL << GPIO_MAX7219_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    gpio_set_level(GPIO_MAX7219_CLK, 0);
    gpio_set_level(GPIO_MAX7219_CS, 1);
    write_register(MATRIX_REGISTER_SHUTDOWN, 0);
    write_config();
    for (uint8_t row = 1; row <= MATRIX_SIZE; ++row) {
        write_register(row, 0);
    }
    write_register(MATRIX_REGISTER_SHUTDOWN, 1);
    s_initialized = true;
    return ESP_OK;
}

esp_err_t matrix_set_power(bool on)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_powered = on;
    write_register(MATRIX_REGISTER_SHUTDOWN, shutdown_value());
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

void matrix_quiet(bool quiet)
{
    if (!s_initialized) {
        return;
    }
    // quiet=true takes the bus lock and keeps it until quiet=false, so no frame is sent while
    // the sonar listens (bus edges and LED scanning both corrupt the echo).
    if (quiet) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_quiet = true;
        write_register(MATRIX_REGISTER_SHUTDOWN, shutdown_value());
    } else {
        s_quiet = false;
        write_register(MATRIX_REGISTER_SHUTDOWN, shutdown_value());
        xSemaphoreGive(s_lock);
    }
}

esp_err_t matrix_render_status(const radar_status_t *status, uint32_t now_ms)
{
    if (!s_initialized || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t frame[MATRIX_SIZE] = {0};
    switch (status->state) {
    case RADAR_STATE_TARGET_LOCKED:
        render_target(frame);
        break;
    case RADAR_STATE_CRITICAL_ALERT:
        render_alert(frame, (now_ms / 150U) % 2U == 0U);
        break;
    case RADAR_STATE_TARGET_LOST:
        render_alert(frame, (now_ms / 300U) % 2U == 0U);
        break;
    case RADAR_STATE_PATROL:
    default:
        render_patrol(frame, status->angle_deg);
        break;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    write_config();
    write_register(MATRIX_REGISTER_SHUTDOWN, shutdown_value());
    for (uint8_t row = 0; row < MATRIX_SIZE; ++row) {
        write_register((uint8_t)(row + 1), frame[row]);
    }
    xSemaphoreGive(s_lock);
    return ESP_OK;
}
