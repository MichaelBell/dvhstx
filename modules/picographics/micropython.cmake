set(MOD_NAME picographics)
string(TOUPPER ${MOD_NAME} MOD_NAME_UPPER)
add_library(usermod_${MOD_NAME} INTERFACE)

get_filename_component(PICOVISION_PATH ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

target_sources(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.cpp
    ${PICOVISION_PATH}/drivers/dvhstx/dvhstx.cpp
    ${PICOVISION_PATH}/drivers/dvhstx/dvi.cpp
    ${PICOVISION_PATH}/drivers/dvhstx/intel_one_mono_2bpp.c
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
    ${PICOVISION_PATH}/libraries/pico_graphics/pico_graphics_pen_dvhstx_rgb565.cpp
    ${PICOVISION_PATH}/libraries/pico_graphics/pico_graphics_pen_dvhstx_p8.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

# MicroPython compiles with -Os by default, these functions are critical path enough that -O2 is worth it (note -O3 is slower in this case)
set_source_files_properties(${PICOVISION_PATH}/drivers/dvhstx/dvhstx.cpp PROPERTIES COMPILE_OPTIONS "-O2")


target_include_directories(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICOVISION_PATH}
    ${PICOVISION_PATH}/drivers/dvhstx
    ${PICOVISION_PATH}/libraries/pico_graphics     # for pico_graphics_dv.hpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics  # for pico_graphics.hpp
#    ${PIMORONI_PICO_PATH}/libraries/pngdec
)

target_compile_definitions(usermod_${MOD_NAME} INTERFACE
    -DMODULE_${MOD_NAME_UPPER}_ENABLED=1
)

if (SUPPORT_WIDE_MODES)
target_compile_definitions(usermod_${MOD_NAME} INTERFACE
    -DSUPPORT_WIDE_MODES=1
)
endif()

target_link_libraries(usermod INTERFACE usermod_${MOD_NAME} hardware_vreg)

set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-discarded-qualifiers"
)
