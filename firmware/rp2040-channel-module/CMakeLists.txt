cmake_minimum_required(VERSION 3.20)

# Pull in Pico SDK (set PICO_SDK_PATH env var or override below)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2040_channel_module C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(rp2040_channel_module
    main.cpp
    FaderScanner.cpp
    EncoderScanner.cpp
    ButtonMatrix.cpp
    LedDriver.cpp
    DisplayDriver.cpp
    Protocol.cpp
    Calibration.cpp
)

# MODULE_ID can be overridden at build time: cmake -DMODULE_ID=1 ..
if(NOT DEFINED MODULE_ID)
    set(MODULE_ID 0)
endif()

target_compile_definitions(rp2040_channel_module PRIVATE
    MODULE_ID=${MODULE_ID}
)

target_include_directories(rp2040_channel_module PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(rp2040_channel_module
    pico_stdlib
    hardware_adc
    hardware_uart
    hardware_i2c
    hardware_pio
    hardware_flash
    hardware_sync
    hardware_gpio
)

pico_add_extra_outputs(rp2040_channel_module)   # generates .uf2, .bin, .elf
pico_enable_stdio_usb(rp2040_channel_module 1)  # USB serial for debug
pico_enable_stdio_uart(rp2040_channel_module 0)
