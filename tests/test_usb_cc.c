#include "usb_cc.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned int s_checks;
static unsigned int s_failures;

static void check(int condition, const char *description)
{
    s_checks++;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        s_failures++;
    }
}

static void test_classify_typical_voltages(void)
{
    /* Rp 56k (default), 22k (1.5 A), 10k (3 A) against Rd 5.1k from 5 V. */
    check(usb_cc_classify(0) == USB_CC_NONE, "0 mV means no supply");
    check(usb_cc_classify(150) == USB_CC_NONE, "150 mV is below detection");
    check(usb_cc_classify(417) == USB_CC_DEFAULT, "417 mV is default USB");
    check(usb_cc_classify(940) == USB_CC_1A5, "940 mV is 1.5 A");
    check(usb_cc_classify(1690) == USB_CC_3A, "1690 mV is 3 A");
    check(usb_cc_classify(2500) == USB_CC_NONE, "2500 mV is out of range");
}

static void test_classify_boundaries(void)
{
    check(usb_cc_classify(199) == USB_CC_NONE, "199 mV is none");
    check(usb_cc_classify(200) == USB_CC_DEFAULT, "200 mV starts default USB");
    check(usb_cc_classify(659) == USB_CC_DEFAULT, "659 mV is default USB");
    check(usb_cc_classify(660) == USB_CC_1A5, "660 mV starts 1.5 A");
    check(usb_cc_classify(1229) == USB_CC_1A5, "1229 mV is 1.5 A");
    check(usb_cc_classify(1230) == USB_CC_3A, "1230 mV starts 3 A");
    check(usb_cc_classify(2199) == USB_CC_3A, "2199 mV is 3 A");
    check(usb_cc_classify(2200) == USB_CC_NONE, "2200 mV is out of range");
}

static void test_best_of_two_lines(void)
{
    check(usb_cc_best(USB_CC_NONE, USB_CC_1A5) == USB_CC_1A5, "plug on CC2 is picked");
    check(usb_cc_best(USB_CC_3A, USB_CC_NONE) == USB_CC_3A, "plug on CC1 is picked");
    check(usb_cc_best(USB_CC_NONE, USB_CC_NONE) == USB_CC_NONE, "no plug stays none");
}

static void test_divider(void)
{
    /* R20 100k to 5V, R21 47k to GND: 5.03 V gives 1.609 V at the pin. */
    check(usb_divider_to_supply_mv(1609, 100, 47) == 5032, "divider restores 5.03 V");
    check(usb_divider_to_supply_mv(1000, 100, 0) == 0, "zero bottom resistor does not divide by zero");
    check(usb_divider_to_supply_mv(0, 100, 47) == 0, "0 V stays 0 V");
}

int main(void)
{
    test_classify_typical_voltages();
    test_classify_boundaries();
    test_best_of_two_lines();
    test_divider();

    if (s_failures != 0U) {
        fprintf(stderr, "%u of %u checks failed\n", s_failures, s_checks);
        return EXIT_FAILURE;
    }
    printf("test_usb_cc: %u checks passed\n", s_checks);
    return EXIT_SUCCESS;
}
