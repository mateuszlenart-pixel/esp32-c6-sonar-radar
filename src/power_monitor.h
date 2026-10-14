#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "usb_cc.h"

typedef struct {
    uint32_t supply_mv;       /* +5V rail measured through the R20/R21 divider */
    uint32_t cc1_mv;          /* voltage on the USB-C CC1 line */
    uint32_t cc2_mv;          /* voltage on the USB-C CC2 line */
    usb_cc_current_t supply;  /* current advertised by the supply */
    bool overcurrent;         /* U2 FLG pin is low (load switch is limiting) */
} power_status_t;

esp_err_t power_monitor_start(void);
bool power_monitor_get(power_status_t *status);
