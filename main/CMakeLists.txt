set(srcs
    main.c
    LiotRfCmd.c
    LiotUart.c
)

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES freertos esp_driver_spi esp_driver_gpio esp_driver_uart esp_ringbuf esp_lora_driver microrl
                       INCLUDE_DIRS ".")
