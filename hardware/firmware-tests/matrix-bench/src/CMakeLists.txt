# Test stanowiskowy: tylko matryca MAX7219 (DIN=GPIO7, CLK=GPIO18, CS=GPIO19).
idf_component_register(SRCS "main.c" "../../../../src/matrix.c"
                       INCLUDE_DIRS "." "../../../../src"
                       PRIV_REQUIRES esp_driver_gpio)
