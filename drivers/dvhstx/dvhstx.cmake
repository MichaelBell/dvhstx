set(DRIVER_NAME dvhstx)

# main DV display driver
add_library(${DRIVER_NAME} INTERFACE)

target_sources(${DRIVER_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/dvi.cpp
  ${CMAKE_CURRENT_LIST_DIR}/${DRIVER_NAME}.cpp
  
  ${CMAKE_CURRENT_LIST_DIR}/intel_one_mono_2bpp.c
  )

target_include_directories(${DRIVER_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(${DRIVER_NAME} INTERFACE pico_stdlib)
