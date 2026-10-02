#include "scan_control.h"

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

static void test_patrol_turns_at_both_ends(void)
{
    int direction = 1;
    int angle = radar_scan_advance(163, &direction, false, 15, 165, 2);
    check(angle == 165, "patrol stops at the maximum angle");
    check(direction == -1, "patrol reverses at the maximum angle");

    angle = radar_scan_advance(angle, &direction, false, 15, 165, 2);
    check(angle == 163, "patrol moves inward after the maximum angle");

    direction = -1;
    angle = radar_scan_advance(17, &direction, false, 15, 165, 2);
    check(angle == 15, "patrol stops at the minimum angle");
    check(direction == 1, "patrol reverses at the minimum angle");
}

static void test_complete_patrol_stays_within_limits(void)
{
    int angle = 15;
    int direction = 1;
    int saw_maximum = 0;
    int returned_to_minimum = 0;
    unsigned int steps = 0;

    while (!returned_to_minimum && steps < 200) {
        const int previous_direction = direction;
        angle = radar_scan_advance(angle, &direction, false, 15, 165, 2);
        steps++;

        check(angle >= 15 && angle <= 165,
              "every angle in the full patrol stays within the configured limits");
        if (angle == 165) {
            saw_maximum = 1;
        }
        if (saw_maximum && angle == 15 && direction == 1 && previous_direction == -1) {
            returned_to_minimum = 1;
        }
    }

    check(saw_maximum, "full patrol reaches the maximum angle");
    check(returned_to_minimum, "full patrol returns to the minimum and reverses");
    check(steps == 150, "full patrol uses 150 two-degree steps");
}

static void test_invalid_parameters_leave_state_unchanged(void)
{
    int direction = 1;
    check(radar_scan_advance(90, &direction, false, 15, 165, 0) == 90,
          "zero step leaves the angle unchanged");
    check(direction == 1, "invalid step leaves the direction unchanged");
    check(radar_scan_advance(90, &direction, false, 165, 15, 2) == 90,
          "reversed limits leave the angle unchanged");
    check(radar_scan_advance(90, NULL, false, 15, 165, 2) == 90,
          "null direction leaves the angle unchanged");
}

static void test_target_lock_pauses_and_resumes_patrol(void)
{
    int direction = 1;
    int angle = radar_scan_advance(90, &direction, true, 15, 165, 2);
    check(angle == 90, "target lock keeps the servo at the current angle");
    check(direction == 1, "target lock preserves the patrol direction");

    angle = radar_scan_advance(angle, &direction, false, 15, 165, 2);
    check(angle == 92, "patrol resumes when the target lock clears");
}

int main(void)
{
    test_patrol_turns_at_both_ends();
    test_complete_patrol_stays_within_limits();
    test_invalid_parameters_leave_state_unchanged();
    test_target_lock_pauses_and_resumes_patrol();

    if (s_failures != 0) {
        fprintf(stderr, "%u of %u checks failed\n", s_failures, s_checks);
        return EXIT_FAILURE;
    }
    printf("All %u radar scan checks passed\n", s_checks);
    return EXIT_SUCCESS;
}
