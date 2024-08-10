cmake_minimum_required(VERSION 3.12)

set(PICO_PLATFORM rp2350)

include(pico_sdk_import.cmake)
include(pimoroni_pico_import.cmake)

# Gooey boilerplate
project(pico_dvhstx_examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

include(pico_dvhstx.cmake)

add_subdirectory(examples/dvhstx)
