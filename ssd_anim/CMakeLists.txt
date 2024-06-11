cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(ssd_anim)

pico_sdk_init()

add_subdirectory(pico-ssd1306 anims)

add_executable(ssd_anim
        main.cpp
        animation_controller.cpp
        anims/idle.c)

target_link_libraries(ssd_anim
        hardware_i2c
        pico_ssd1306)



pico_add_extra_outputs(ssd_anim)

# enable usb output, disable uart output
pico_enable_stdio_usb(ssd_anim 1)
pico_enable_stdio_uart(ssd_anim 0)