idf_component_register(SRCS "firmware_main.c"
                       REQUIRES esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_driver_uart esp_driver_rmt esp_timer)
