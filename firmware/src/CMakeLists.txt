FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_LIST_DIR}/*.c)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS
        "../include"
        "../include/measurements"
        "../include/indicators"
        "../include/display"
        "../include/network"
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_adc
        esp_timer
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        lwip
)
