set(srcs
    main.c
    LiotRfCmd.c
    LiotUsbJtag.c
)

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES freertos esp_timer esp_driver_spi esp_driver_gpio esp_driver_usb_serial_jtag esp_lora_driver microrl
                       INCLUDE_DIRS ".")
