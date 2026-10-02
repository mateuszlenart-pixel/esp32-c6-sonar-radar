#include "detector.h"

#include <stdio.h>
#include <stdlib.h>

#include "app_config.h"

static unsigned int s_checks;
static unsigned int s_failures;

static void check(bool condition, const char *description)
{
    s_checks++;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        s_failures++;
    }
}

static radar_detector_events_t sample(radar_detector_t *detector,
                                      bool valid,
                                      uint32_t distance_cm,
                                      uint32_t now_ms)
{
    return radar_detector_update(detector, valid, distance_cm, now_ms);
}

static void test_target_confirmation_and_invalid_sample(void)
{
    radar_detector_t detector;
    radar_detector_init(&detector);

    check(sample(&detector, true, 50, 0) == RADAR_EVENT_NONE,
          "one near reading does not acquire a target");
    check(sample(&detector, true, 49, 80) == RADAR_EVENT_NONE,
          "two near readings do not acquire a target");
    check(sample(&detector, false, 0, 160) == RADAR_EVENT_NONE,
          "an invalid reading does not acquire a target");
    check(sample(&detector, true, 50, 240) == RADAR_EVENT_NONE,
          "invalid reading resets consecutive target confirmations");
    check(sample(&detector, true, 48, 320) == RADAR_EVENT_NONE,
          "two consecutive readings after reset still do not acquire a target");
    check(sample(&detector, true, 47, 400) == RADAR_EVENT_TARGET_ACQUIRED,
          "third consecutive near reading acquires a target");
    check(detector.state == RADAR_STATE_TARGET_LOCKED,
          "acquired target enters locked state");
}

static void test_target_hysteresis_and_lost_state(void)
{
    radar_detector_t detector;
    radar_detector_init(&detector);
    sample(&detector, true, 50, 0);
    sample(&detector, true, 50, 80);
    sample(&detector, true, 50, 160);

    check(sample(&detector, true, 54, 240) == RADAR_EVENT_NONE,
          "release threshold keeps the target active");
    check(detector.target_active, "target remains active below release threshold");
    check(sample(&detector, true, 56, 1739) == RADAR_EVENT_NONE,
          "target is not lost before the timeout");
    check(sample(&detector, true, 56, 1740) == RADAR_EVENT_TARGET_LOST,
          "target is lost when the timeout expires");
    check(detector.state == RADAR_STATE_TARGET_LOST,
          "target lost state is entered");
    sample(&detector, false, 0, 2739);
    check(detector.state == RADAR_STATE_TARGET_LOST,
          "target lost state remains visible for its display interval");
    sample(&detector, false, 0, 2740);
    check(detector.state == RADAR_STATE_PATROL,
          "patrol state resumes after the lost display interval");
}

static void test_critical_alert_hysteresis(void)
{
    radar_detector_t detector;
    radar_detector_init(&detector);
    sample(&detector, true, 50, 0);
    sample(&detector, true, 50, 80);
    sample(&detector, true, 50, 160);

    check(sample(&detector, true, 20, 240) == RADAR_EVENT_NONE,
          "one critical reading does not raise the alert");
    check(sample(&detector, true, 19, 320) == RADAR_EVENT_CRITICAL_ENTERED,
          "second consecutive critical reading raises the alert");
    check(detector.state == RADAR_STATE_CRITICAL_ALERT,
          "critical alert becomes the current state");
    check(sample(&detector, true, 26, 400) == RADAR_EVENT_NONE,
          "one reading above critical release does not clear the alert");
    check(sample(&detector, true, 25, 480) == RADAR_EVENT_NONE,
          "the exact release threshold does not clear the alert");
    check(sample(&detector, true, 26, 560) == RADAR_EVENT_NONE,
          "one new reading above the threshold starts a fresh clear sequence");
    check(sample(&detector, true, 26, 640) == RADAR_EVENT_CRITICAL_CLEARED,
          "second reading above critical release clears the alert");
    check(detector.state == RADAR_STATE_TARGET_LOCKED,
          "cleared critical alert returns to target locked state");
}

static void test_timeout_across_clock_wrap(void)
{
    radar_detector_t detector;
    radar_detector_init(&detector);
    const uint32_t start_ms = UINT32_MAX - 100U;
    sample(&detector, true, 40, start_ms);
    sample(&detector, true, 40, start_ms + 80U);
    sample(&detector, true, 40, start_ms + 160U);

    const uint32_t last_seen_ms = start_ms + 160U;
    check(sample(&detector,
                 false,
                 0,
                 last_seen_ms + RADAR_TARGET_LOST_TIMEOUT_MS - 1U) == RADAR_EVENT_NONE,
          "target timeout remains valid across the millisecond counter wrap");
    check(sample(&detector,
                 false,
                 0,
                 last_seen_ms + RADAR_TARGET_LOST_TIMEOUT_MS) == RADAR_EVENT_TARGET_LOST,
          "target expires at the timeout across the millisecond counter wrap");
}

int main(void)
{
    test_target_confirmation_and_invalid_sample();
    test_target_hysteresis_and_lost_state();
    test_critical_alert_hysteresis();
    test_timeout_across_clock_wrap();

    if (s_failures != 0) {
        fprintf(stderr, "%u of %u checks failed\n", s_failures, s_checks);
        return EXIT_FAILURE;
    }

    printf("All %u radar detector checks passed\n", s_checks);
    return EXIT_SUCCESS;
}
