idf_component_register(SRCS "main.c" "radar.c" "servo.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_ledc)
