#include "usb_cc.h"

/* Sink detection thresholds from the USB Type-C specification for Rd = 5.1 kOhm:
 * default USB 0.25-0.61 V, 1.5 A 0.70-1.16 V, 3 A 1.31-2.04 V. Boundaries are
 * placed midway between the ranges so that ADC and divider tolerance do not matter. */
#define CC_NONE_MAX_MV 200U
#define CC_DEFAULT_MAX_MV 660U
#define CC_1A5_MAX_MV 1230U
#define CC_3A_MAX_MV 2200U

usb_cc_current_t usb_cc_classify(uint32_t cc_mv)
{
    if (cc_mv < CC_NONE_MAX_MV) {
        return USB_CC_NONE;
    }
    if (cc_mv < CC_DEFAULT_MAX_MV) {
        return USB_CC_DEFAULT;
    }
    if (cc_mv < CC_1A5_MAX_MV) {
        return USB_CC_1A5;
    }
    if (cc_mv < CC_3A_MAX_MV) {
        return USB_CC_3A;
    }
    return USB_CC_NONE;
}

usb_cc_current_t usb_cc_best(usb_cc_current_t cc1, usb_cc_current_t cc2)
{
    return cc1 > cc2 ? cc1 : cc2;
}

uint32_t usb_divider_to_supply_mv(uint32_t pin_mv, uint32_t top, uint32_t bottom)
{
    if (bottom == 0U) {
        return 0U;
    }
    return (uint32_t)(((uint64_t)pin_mv * (top + bottom) + bottom / 2U) / bottom);
}

const char *usb_cc_name(usb_cc_current_t level)
{
    switch (level) {
    case USB_CC_DEFAULT:
        return "default USB (500/900 mA)";
    case USB_CC_1A5:
        return "1.5 A";
    case USB_CC_3A:
        return "3 A";
    case USB_CC_NONE:
    default:
        return "none";
    }
}
