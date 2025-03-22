
include(drivers/aps6404/aps6404)
include(drivers/aps6408/aps6408)
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
    ${PIMORONI_PICO_PATH}/libraries/jpegdec
)

target_link_libraries(${LIB_NAME} INTERFACE dvhstx pico_stdlib hardware_i2c hardware_dma)
