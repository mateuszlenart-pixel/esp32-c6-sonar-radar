#pragma once

#include <stdint.h>

/* Current advertised by the USB-C supply on the CC line (sink side, 5.1 kOhm Rd). */
typedef enum {
    USB_CC_NONE = 0, /* no supply attached or voltage out of range */
    USB_CC_DEFAULT,  /* default USB power: 500/900 mA, too little for the radar */
    USB_CC_1A5,      /* 1.5 A advertised */
    USB_CC_3A,       /* 3 A advertised */
} usb_cc_current_t;

/* Classifies the voltage on a CC pin (millivolts, measured at the connector). */
usb_cc_current_t usb_cc_classify(uint32_t cc_mv);

/* The better of the two CC lines (the plug connects only one of them). */
usb_cc_current_t usb_cc_best(usb_cc_current_t cc1, usb_cc_current_t cc2);

/* Converts the voltage at the ADC pin back to the supply voltage through the
 * divider (top = resistor to the supply, bottom = resistor to GND, same unit). */
uint32_t usb_divider_to_supply_mv(uint32_t pin_mv, uint32_t top, uint32_t bottom);

/* Short human-readable name for logs. */
const char *usb_cc_name(usb_cc_current_t level);
