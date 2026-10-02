idf_component_register(SRCS "main.c" "radar.c" "servo.c" "sonar.c" "sonar_task.c" "detector.c" "logic.c" "oled.c" "ui_task.c" "matrix.c" "buzzer.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_timer esp_rom)
