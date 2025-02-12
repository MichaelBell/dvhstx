# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD adafruit_feather_rp2350 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pimoroni_pico_import.cmake)
include(drivers/dvhstx/dvhstx)

set(LIB_NAME pico_dvhstx)
add_library(${LIB_NAME} INTERFACE)

target_sources(${LIB_NAME} INTERFACE
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
#    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dvhstx_rgb888.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dvhstx_rgb565.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dvhstx_p8.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

target_include_directories(${LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics     # for pico_graphics_dv.hpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics  # for pico_graphics.hpp
    ${PIMORONI_PICO_PATH}/libraries/pngdec
)

target_link_libraries(${LIB_NAME} INTERFACE dvhstx pico_stdlib hardware_i2c hardware_dma)
