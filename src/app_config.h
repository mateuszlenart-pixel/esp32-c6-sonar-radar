#pragma once

#define RADAR_MIN_ANGLE_DEG 15
#define RADAR_MAX_ANGLE_DEG 165

#define RADAR_TARGET_DISTANCE_CM 50
#define RADAR_CRITICAL_DISTANCE_CM 20
#define RADAR_TARGET_RELEASE_DISTANCE_CM 55
#define RADAR_CRITICAL_RELEASE_DISTANCE_CM 25
#define RADAR_TARGET_CONFIRM_COUNT 3
#define RADAR_CRITICAL_CONFIRM_COUNT 2
#define RADAR_CRITICAL_CLEAR_COUNT 2
#define RADAR_TARGET_LOST_TIMEOUT_MS 1500
#define RADAR_TARGET_LOST_DISPLAY_MS 1000

#define SERVO_PWM_FREQUENCY_HZ 50
#define SERVO_PWM_RESOLUTION_BITS 14
#define SERVO_MIN_PULSE_US 1000
#define SERVO_MAX_PULSE_US 2000
#define RADAR_STEP_PERIOD_MS 80
#define RADAR_STEP_DEGREES 2
#define RADAR_SERVO_SETTLE_MS 40
#define SONAR_TRIGGER_PULSE_US 10
#define SONAR_RISE_TIMEOUT_US 30000
#define SONAR_ECHO_TIMEOUT_US 25000
// Shortest ECHO pulse accepted (about 1.7 cm); shorter pulses are treated as noise.
#define SONAR_MIN_ECHO_US 100
// Median over the last N valid readings rejects isolated false echoes (odd number).
#define SONAR_MEDIAN_WINDOW 5
#define SONAR_POLL_INTERVAL_US 5
#define RADAR_TASK_STACK_SIZE 4096
#define RADAR_TASK_PRIORITY 4
#define RADAR_LOGIC_TASK_STACK_SIZE 4096
#define RADAR_LOGIC_TASK_PRIORITY 6
#define RADAR_MEASUREMENT_QUEUE_LENGTH 4
#define RADAR_SONAR_TASK_STACK_SIZE 3072
#define RADAR_SONAR_TASK_PRIORITY 5
#define SONAR_REQUEST_QUEUE_LENGTH 1
#define RADAR_UI_TASK_STACK_SIZE 4096
#define RADAR_UI_TASK_PRIORITY 3
#define RADAR_UI_REFRESH_MS 50
#define RADAR_NETWORK_TASK_STACK_SIZE 4096
#define RADAR_NETWORK_TASK_PRIORITY 2
#define RADAR_WIFI_RETRY_DELAY_MS 3000
#define OLED_I2C_PORT 0
#define OLED_I2C_ADDRESS 0x3C
#define OLED_I2C_FREQUENCY_HZ 100000
#define MAX7219_INTENSITY 2
#define BUZZER_PWM_FREQUENCY_HZ 2000
#define BUZZER_PWM_RESOLUTION_BITS 10

// Avoid the GPIO4/GPIO5 strapping pins on ESP32-C6-DevKitC-1.
#define GPIO_SERVO 10
#define GPIO_SONAR_TRIG 11
#define GPIO_SONAR_ECHO 6
#define GPIO_MAX7219_DIN 7
#define GPIO_MAX7219_CLK 18
#define GPIO_MAX7219_CS 19
#define GPIO_OLED_SDA 20
#define GPIO_OLED_SCL 21
// Output for the transistor driver (Q2) on the PCB; do not connect a buzzer directly.
#define GPIO_BUZZER 22
// 0 = passive buzzer (driven with a PWM tone, distinct alert tones),
// 1 = active buzzer (built-in oscillator, driven only on/off, one fixed tone).
#define BUZZER_ACTIVE 0
// Push button to GND (internal pull-up): toggles the device between ON and standby.
#define GPIO_POWER_BUTTON 2
#define POWER_BUTTON_POLL_MS 20
#define POWER_BUTTON_DEBOUNCE_MS 60
#define RADAR_POWER_TASK_STACK_SIZE 3072
#define RADAR_POWER_TASK_PRIORITY 3

// Power supervision on the PCB (hardware/pcb): +5V divider R20 100k / R21 47k on GPIO0.
// POWER_MONITOR_USB_C 1 = the earlier USB-C board: CC1/CC2 of the power jack on GPIO1/GPIO3 (100k + 100 nF)
// and the load-switch fault flag on GPIO23. 0 = DC jack board (through-hole version): those pins are free
// and only the 5 V rail is measured.
#define POWER_MONITOR_USB_C 0
#define GPIO_V5_SENSE 0
#define GPIO_CC1_SENSE 1
#define GPIO_CC2_SENSE 3
#define GPIO_POWER_FAULT 23
#define V5_DIVIDER_TOP_KOHM 100
#define V5_DIVIDER_BOTTOM_KOHM 47
#define V5_LOW_WARN_MV 4500
#define POWER_MONITOR_SAMPLES 8
#define POWER_MONITOR_PERIOD_MS 1000
#define POWER_MONITOR_REPORT_S 30
#define RADAR_POWER_MONITOR_STACK_SIZE 3072
#define RADAR_POWER_MONITOR_PRIORITY 2
