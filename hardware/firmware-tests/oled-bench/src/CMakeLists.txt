# Test stanowiskowy: tylko OLED (I2C GPIO20/21). Używa oled.c i app_config.h z głównego firmware.
idf_component_register(SRCS "main.c" "../../../../src/oled.c"
                       INCLUDE_DIRS "." "../../../../src"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer)
