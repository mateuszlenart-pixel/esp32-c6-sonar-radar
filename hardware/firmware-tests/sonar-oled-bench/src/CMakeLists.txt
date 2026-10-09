# Test stanowiskowy: HC-SR04 (GPIO11/GPIO6) + OLED SH1106 (GPIO20/GPIO21).
# Używa sonar.c, oled.c i app_config.h z głównego firmware.
idf_component_register(SRCS "main.c" "../../../../src/sonar.c" "../../../../src/oled.c"
                       INCLUDE_DIRS "." "../../../../src"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_rom)
