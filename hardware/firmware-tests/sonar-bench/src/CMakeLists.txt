# Test stanowiskowy: tylko HC-SR04. Używa sonar.c i app_config.h z głównego firmware.
idf_component_register(SRCS "main.c" "../../../../src/sonar.c"
                       INCLUDE_DIRS "." "../../../../src"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_mcpwm esp_timer esp_rom)
