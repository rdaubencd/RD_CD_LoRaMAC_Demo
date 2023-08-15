#
# COPYRIGHT 2022 CONNECTED DEVELOPMENT, A DIVISION OF
# EXPONENTIAL TECHNOLOGY GROUP.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Assume the nrf52840-DK build folder is named "build".
# If a different board is built, such as IotaComm, use a different folder name.

file(RELATIVE_PATH Build_Dir ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# If building for the nrf52840-DK, define the shield.
if(${Build_Dir} STREQUAL "build")
   message(STATUS "Building \"${Build_Dir}\" -- Using the shield.")
   set(SHIELD semtech_sx1262_lora)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_pingpong)

FILE(GLOB app_sources Application/*.c)
target_sources(app PRIVATE ${app_sources})

# Add additional includes for main.c.
target_include_directories(app PRIVATE
    ../ConDev/modules/lib/loramac-node/src/radio)

# If building for the nrf52840-DK, enable the LEDs.
if(${Build_Dir} STREQUAL "build")
   message(STATUS "Building \"${Build_Dir}\" -- Enabling LEDs.")
   target_compile_definitions(app PRIVATE USE_LEDS)
endif()

