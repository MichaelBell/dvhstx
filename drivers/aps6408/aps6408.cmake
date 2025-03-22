set(DRIVER_NAME aps6408)
add_library(${DRIVER_NAME} INTERFACE)

target_sources(${DRIVER_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/aps6408.cpp
)

pico_generate_pio_header(${DRIVER_NAME} ${CMAKE_CURRENT_LIST_DIR}/aps6408.pio)

target_include_directories(${DRIVER_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${DRIVER_NAME} INTERFACE pico_stdlib hardware_pio hardware_dma hardware_irq)
